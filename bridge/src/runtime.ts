// runtime.json is re-read on every call (never cached), since devbench's port can
// change across a game restart.

import { readFileSync, statSync } from "node:fs";
import { join } from "node:path";

export interface Target {
  /** Human-readable label for error messages ("SE", "VR", or the --install path). */
  label: string;
  /** Directory containing runtime.json — the Data-relative path or its %LOCALAPPDATA% mirror. */
  runtimeDir: string;
  /** Known candidate directories where runtime.json may appear once the game launches. */
  candidates?: string[];
}

interface RuntimeJson {
  port: number;
  [key: string]: unknown;
}

// Best-effort default Steam library locations; --install covers anything else.
const DEFAULT_SE_INSTALLS = [
  "C:/Program Files (x86)/Steam/steamapps/common/Skyrim Special Edition",
  "C:/SteamLibrary/steamapps/common/Skyrim Special Edition",
  "D:/SteamLibrary/steamapps/common/Skyrim Special Edition",
  "E:/SteamLibrary/steamapps/common/Skyrim Special Edition",
];
const DEFAULT_VR_INSTALLS = [
  "C:/Program Files (x86)/Steam/steamapps/common/SkyrimVR",
  "C:/SteamLibrary/steamapps/common/SkyrimVR",
  "D:/SteamLibrary/steamapps/common/SkyrimVR",
  "E:/SteamLibrary/steamapps/common/SkyrimVR",
];

function runtimeDirFor(installPath: string): string {
  return join(installPath, "Data", "SKSE", "Plugins", "devbench");
}

// Must match Server.cpp's ExternalStateDir() exactly: %LOCALAPPDATA%\devbench\<se|vr>,
// reachable even under a VFS mod manager (MO2) where Data/SKSE/Plugins/devbench is virtual.
function localAppDataDevbenchDir(game: "se" | "vr"): string | undefined {
  const localAppData = process.env.LOCALAPPDATA;
  return localAppData ? join(localAppData, "devbench", game) : undefined;
}

// runtime.json survives after the game exits, so picking the first readable
// candidate can pin a stale install over a live one; pick the most recently
// written file instead (devbench rewrites it fresh on every boot).
function freshestExisting(candidates: string[]): string | undefined {
  let best: { dir: string; mtimeMs: number } | undefined;
  for (const dir of candidates) {
    try {
      const mtimeMs = statSync(join(dir, "runtime.json")).mtimeMs;
      if (!best || mtimeMs > best.mtimeMs) best = { dir, mtimeMs };
    } catch (e) {
      if ((e as NodeJS.ErrnoException).code !== "ENOENT") throw e;
    }
  }
  return best?.dir;
}

/** Whether `game` is a supported --game value. */
export function isSupportedGame(game: string | undefined): game is "se" | "vr" {
  return game === "se" || game === "vr";
}

/** Resolve a Target from parsed CLI args. Throws with a clear message if none found. */
export function resolveTarget(args: {
  game?: string;
  install?: string;
}): Target {
  if (args.install) {
    const dir = runtimeDirFor(args.install);
    return { label: args.install, runtimeDir: dir, candidates: [dir] };
  }
  if (isSupportedGame(args.game)) {
    const candidates = (
      args.game === "se" ? DEFAULT_SE_INSTALLS : DEFAULT_VR_INSTALLS
    ).map(runtimeDirFor);
    const localAppDataDir = localAppDataDevbenchDir(args.game);
    if (localAppDataDir) candidates.push(localAppDataDir);
    const found = freshestExisting(candidates);
    // If no runtime.json exists yet (game not yet started), pick the first candidate directory that
    // exists on disk, or fall back to the primary default candidate. This allows the bridge to start
    // up and serve the fallback tools list instead of exiting immediately.
    const fallbackDir =
      candidates.find((d) => {
        try {
          return statSync(d).isDirectory();
        } catch {
          return false;
        }
      }) ?? candidates[0];

    return {
      label: args.game.toUpperCase(),
      runtimeDir: found ?? fallbackDir,
      candidates,
    };
  }
  throw new Error("devbench-bridge requires --game se|vr or --install <path>.");
}

/** Read the live port + base URL for a target, fresh every call. */
export function resolveBaseUrl(target: Target): string {
  const dir =
    (target.candidates ? freshestExisting(target.candidates) : undefined) ??
    target.runtimeDir;
  const raw = readFileSync(join(dir, "runtime.json"), "utf-8");
  const parsed = JSON.parse(raw) as RuntimeJson;
  if (
    !Number.isInteger(parsed.port) ||
    parsed.port < 1 ||
    parsed.port > 65535
  ) {
    throw new Error(
      `runtime.json at ${dir} has no valid "port" field (1-65535).`,
    );
  }
  return `http://127.0.0.1:${parsed.port}`;
}
