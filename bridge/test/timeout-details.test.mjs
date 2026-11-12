import assert from "node:assert/strict";
import { mkdtemp, mkdir, writeFile, rm } from "node:fs/promises";
import { createServer } from "node:http";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StdioClientTransport } from "@modelcontextprotocol/sdk/client/stdio.js";

// Exercise the real stdio bridge against a simulated REST server, without Skyrim.
test("bridge preserves timeout outcomes, existing errors and successful replies", async (t) => {
  const install = await mkdtemp(join(tmpdir(), "devbench-bridge-"));
  t.after(() => rm(install, { recursive: true, force: true }));
  const runtime = join(install, "Data", "SKSE", "Plugins", "devbench");
  await mkdir(runtime, { recursive: true });
  const server = createServer((req, res) => {
    res.setHeader("Content-Type", "application/json");
    if (req.url === "/api/tools") {
      res.end(
        JSON.stringify({
          identity: {
            instanceId: "test-instance",
            build: { gitCommit: "test", dirty: true },
          },
          tools: [
            {
              name: "probe",
              description: "test",
              inputSchema: { type: "object" },
            },
          ],
        }),
      );
      return;
    }
    if (req.url.endsWith("/success")) {
      res.end(JSON.stringify({ lines: ["own capture"], completed: true }));
      return;
    }
    if (req.url.endsWith("/badarg")) {
      res.writeHead(400);
      res.end(JSON.stringify({ error: "bad argument", code: 400 }));
      return;
    }
    const started = req.url.endsWith("/running");
    res.writeHead(504);
    res.end(
      JSON.stringify({
        error: "unchanged timeout",
        code: 504,
        details: {
          kind: "main_thread_timeout",
          taskStarted: started,
          abandoned: !started,
          outcome: started ? "unknown" : "not_started",
        },
      }),
    );
  });
  await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
  t.after(() => new Promise((resolve) => server.close(resolve)));
  await writeFile(
    join(runtime, "runtime.json"),
    JSON.stringify({ port: server.address().port }),
  );
  const client = new Client({ name: "test", version: "1" });
  const transport = new StdioClientTransport({
    command: process.execPath,
    args: [
      new URL("../dist/index.js", import.meta.url).pathname,
      "--install",
      install,
    ],
    stderr: "pipe",
  });
  t.after(() => client.close());
  await client.connect(transport);
  assert.equal((await client.listTools()).tools[0].name, "probe");
  for (const started of [false, true]) {
    const result = await client.callTool({
      name: started ? "running" : "queued",
      arguments: {},
    });
    assert.equal(result.isError, true);
    const body = JSON.parse(result.content[0].text);
    assert.equal(body.ok, false);
    assert.equal(body.reason, "devbench returned 504: unchanged timeout");
    assert.equal(body.error, "unchanged timeout");
    assert.equal(body.code, 504);
    assert.deepEqual(body.details, {
      kind: "main_thread_timeout",
      taskStarted: started,
      abandoned: !started,
      outcome: started ? "unknown" : "not_started",
    });
  }
  const failure = await client.callTool({ name: "badarg", arguments: {} });
  assert.equal(failure.isError, true);
  assert.deepEqual(JSON.parse(failure.content[0].text), {
    ok: false,
    reason: "devbench returned 400: bad argument",
  });
  const success = await client.callTool({ name: "success", arguments: {} });
  assert.notEqual(success.isError, true);
  assert.deepEqual(JSON.parse(success.content[0].text), {
    lines: ["own capture"],
    completed: true,
  });
});
