"""Build provenance must refresh without reconfiguring and never borrow a parent repo's identity."""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

SOURCE = Path(__file__).resolve().parents[1]


class BuildInfoTest(unittest.TestCase):
    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory(prefix="devbench-build-info-")
        self.root = Path(self.scratch.name)
        self.addCleanup(self.scratch.cleanup)
        self.repo = self.root / "repo"
        (self.repo / "src").mkdir(parents=True)
        shutil.copy2(SOURCE / "src/BuildInfo.h.in", self.repo / "src/BuildInfo.h.in")
        (self.repo / "xmake.lua").write_text('set_project("fixture")\n')
        self.git("init", "-q")
        self.git("config", "user.name", "Test")
        self.git("config", "user.email", "test@example.invalid")
        self.git("add", ".")
        self.git("commit", "-qm", "initial")

    def git(self, *args):
        return subprocess.check_output(["git", "-C", str(self.repo), *args], text=True).strip()

    def generate(self, source=None):
        header = self.root / "BuildInfo.h"
        subprocess.run(["cmake", "-DSOURCE_DIR=" + str(source or self.repo),
                        "-DOUTPUT_FILE=" + str(header), "-P", str(SOURCE / "xmake/write-build-info.cmake")], check=True)
        return header.read_text()

    def test_clean_dirty_and_new_commit_refresh_in_same_build_directory(self):
        revision = self.git("rev-parse", "HEAD")
        self.assertIn('"' + revision + '"', self.generate())
        self.assertIn("DEVBENCH_GIT_DIRTY 0", self.generate())
        (self.repo / "src/BuildInfo.h.in").write_text((self.repo / "src/BuildInfo.h.in").read_text() + "// edited\n")
        self.assertIn("DEVBENCH_GIT_DIRTY 1", self.generate())
        self.git("add", ".")
        self.git("commit", "-qm", "change")
        self.assertNotEqual(revision, self.git("rev-parse", "HEAD"))
        self.assertIn('"' + self.git("rev-parse", "HEAD") + '"', self.generate())
        self.assertIn("DEVBENCH_GIT_DIRTY 0", self.generate())

    def test_untracked_source_marks_build_dirty(self):
        (self.repo / "new.cpp").write_text("// untracked source\n")
        self.assertIn("DEVBENCH_GIT_DIRTY 1", self.generate())

    def test_ignored_build_output_does_not_mark_source_dirty(self):
        (self.repo / ".gitignore").write_text("build/\n")
        self.git("add", ".")
        self.git("commit", "-qm", "ignore build")
        (self.repo / "build").mkdir()
        (self.repo / "build/artifact").write_text("output")
        self.assertIn("DEVBENCH_GIT_DIRTY 0", self.generate())

    def test_source_archive_inside_another_repo_has_unknown_provenance(self):
        archive = self.repo / "archive"
        (archive / "src").mkdir(parents=True)
        shutil.copy2(SOURCE / "src/BuildInfo.h.in", archive / "src/BuildInfo.h.in")
        header = self.generate(archive)
        self.assertIn('DEVBENCH_GIT_COMMIT ""', header)
        self.assertIn("DEVBENCH_GIT_DIRTY -1", header)

    def test_repeated_generation_does_not_touch_unchanged_header(self):
        self.generate()
        header = self.root / "BuildInfo.h"
        previous = header.stat().st_mtime_ns
        self.generate()
        self.assertEqual(previous, header.stat().st_mtime_ns)


    def test_xmake_hook_refreshes_clean_and_dirty_provenance(self):
        if not shutil.which("xmake"):
            self.skipTest("xmake unavailable")
        source = (SOURCE / "xmake.lua").read_text()
        start = source.index("before_build(function(target)\n    local commit, dirty")
        end = source.index("\nend)", start) + len("\nend)")
        output = self.root / "xmake-generated"
        script = self.root / "exercise-hook.lua"
        script.write_text("function main()\nlocal callback\nlocal before_build = function(fn) callback = fn end\n" +
                          source[start:end] + "\ncallback({autogendir = function() return " +
                          json.dumps(str(output)) + " end})\nend\n")
        def generate():
            subprocess.run(["xmake", "lua", str(script)], cwd=self.repo, check=True)
            return (output / "build-info/BuildInfo.h").read_text()
        self.assertIn('"' + self.git("rev-parse", "HEAD") + '"', generate())
        self.assertIn("DEVBENCH_GIT_DIRTY 0", generate())
        (self.repo / "untracked.cpp").write_text("// new source\n")
        self.assertIn("DEVBENCH_GIT_DIRTY 1", generate())


if __name__ == "__main__":
    unittest.main()
