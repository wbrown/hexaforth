{
  description = "hexaforth — stack machine, Forth OS, and QEMU AArch64 baremetal environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
  };

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" "x86_64-darwin" "aarch64-darwin" ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
    in
    {
      devShells = forAllSystems (system:
        let
          pkgs = import nixpkgs { inherit system; };
          gforthCompat = pkgs.writeShellScriptBin "gforth" ''
            exec ${pkgs.gforth}/bin/gforth -e "require libcc.fs also c-lib" "$@"
          '';
          includePath = pkgs.lib.makeSearchPathOutput "dev" "include" [
            pkgs.libtool
            pkgs.unicorn
            pkgs.gforth
          ];
          libPath = pkgs.lib.makeLibraryPath [
            pkgs.libtool.lib
            pkgs.unicorn
          ];
        in
        {
          default = pkgs.mkShell {
            packages = [ gforthCompat ] ++ (with pkgs; [
              cmake
              gnumake
              gcc
              git
              curl
              jq
              libtool
              libtool.lib
              unicorn
              # Unwrapped clang is needed so `clang --target=aarch64-none-elf -c`
              # assembles the baremetal AArch64 kernels without Nix's host cc-wrapper
              # injecting x86_64 target flags.
              llvmPackages.clang-unwrapped
              lld
              llvm
              qemu
              verilator
              yosys
              nextpnr
              trellis
            ]);

            CMAKE_INCLUDE_PATH = includePath;
            CMAKE_LIBRARY_PATH = libPath;
            CPATH = includePath;
            LIBRARY_PATH = libPath;

            shellHook = ''
              if [ -z "''${CLAUDECODE:-}" ] && { [ -n "''${PI_CODING_AGENT:-}" ] || [ -n "''${PI_SESSION_ID:-}" ] || [ -d "$HOME/.pi" ]; }; then
                mkdir -p .pi/extensions
                cat > .pi/extensions/agent-check.ts <<'EOF'
import { execFileSync } from "node:child_process";
import * as fs from "node:fs";
import type { ExtensionAPI } from "@earendil-works/pi-coding-agent";
import { createBashTool } from "@earendil-works/pi-coding-agent";

function verifyDiagnosticInvariants(env: NodeJS.ProcessEnv): void {
  // 1. Verify disasm_hex command format in test/deskvm.c has a two-place range (FROM TO)
  if (fs.existsSync("test/deskvm.c")) {
    const deskvm = fs.readFileSync("test/deskvm.c", "utf8");
    const disasmLines = deskvm.split("\n").filter((l) => l.includes("disasm_hex-"));
    for (const line of disasmLines) {
      if (!/disasm_hex-%d\s+%s\s+0x%04x\s+0x%04x/.test(line)) {
        throw new Error(
          `test/deskvm.c diagnostic command must pass a two-place range (0x%04x 0x%04x) to disasm_hex:\n  ${line.trim()}`
        );
      }
    }
    if (deskvm.includes('"(boot)"')) {
      throw new Error(
        `test/deskvm.c must not hardcode "(boot)" when desk_checking is NULL; desk_begins() sets desk_checking during boot.`
      );
    }
  }

  // 2. Verify test/deskcheck.c single-source-of-truth invariants
  if (fs.existsSync("test/deskcheck.c")) {
    const deskcheck = fs.readFileSync("test/deskcheck.c", "utf8");
    const pbmMatches = deskcheck.match(/\.pbm/g) || [];
    if (pbmMatches.length !== 1) {
      throw new Error(
        `test/deskcheck.c formats ".pbm" in ${pbmMatches.length} places (expected 1, in look()). Keep the path in look() instead of formatting it twice.`
      );
    }
    const titleShows = (deskcheck.match(/shows\(TITLES\[/g) || []).length;
    if (titleShows !== 1) {
      throw new Error(
        `test/deskcheck.c calls shows(TITLES[...]) in ${titleShows} places (expected 1, in title_shown()).`
      );
    }
    const closeShows = (deskcheck.match(/shows\("x",/g) || []).length;
    if (closeShows !== 1) {
      throw new Error(
        `test/deskcheck.c calls shows("x", ...) in ${closeShows} places (expected 1, in x_shown()).`
      );
    }
    if (deskcheck.includes("lines are unhighlighted")) {
      throw new Error(
        `test/deskcheck.c must not call menu_lines() count "unhighlighted"; use "where they should be".`
      );
    }
  }

  // 3. Deterministically test the disasm_hex command format and the failure screen path output
  if (fs.existsSync("build/cmake/disasm_hex-64") && fs.existsSync("build/64/nuc.hex")) {
    execFileSync("build/cmake/disasm_hex-64", ["build/64/nuc.hex", "0x1234", "0x1234", "--no-color"], {
      stdio: "pipe",
      encoding: "utf8",
      env,
    });
  }
}

export default function (pi: ExtensionAPI) {
  let editedInRun = false;
  let checkedInRun = false;
  let devEnv: NodeJS.ProcessEnv | null = null;

  function getDevEnv(): NodeJS.ProcessEnv {
    if (devEnv) return devEnv;
    if (process.env.IN_NIX_SHELL) {
      devEnv = process.env;
      return devEnv;
    }
    try {
      const out = execFileSync("nix", ["develop", "-c", "env", "-0"], {
        stdio: ["ignore", "pipe", "pipe"],
        encoding: "utf8",
      });
      const env: NodeJS.ProcessEnv = { ...process.env };
      for (const entry of out.split("\0")) {
        const eq = entry.indexOf("=");
        if (eq > 0) {
          const k = entry.slice(0, eq);
          const v = entry.slice(eq + 1);
          if (k !== "SHLVL" && k !== "_" && k !== "PWD" && k !== "OLDPWD") {
            env[k] = v;
          }
        }
      }
      devEnv = env;
    } catch {
      devEnv = process.env;
    }
    return devEnv;
  }

  const localCwd = process.cwd();
  const nixBash = createBashTool(localCwd, {
    spawnHook: (ctx) => ({
      ...ctx,
      env: { ...ctx.env, ...getDevEnv() },
    }),
  });
  pi.registerTool(nixBash);

  pi.on("agent_start", () => {
    checkedInRun = false;
  });

  pi.on("tool_call", (event) => {
    if (event.toolName === "bash" && typeof (event as any).input?.command === "string") {
      try {
        execFileSync("bash", ["./.claude/hooks/validate-bash.sh"], {
          input: JSON.stringify({ tool_input: { command: (event as any).input.command } }),
          stdio: ["pipe", "pipe", "pipe"],
          encoding: "utf8",
          env: getDevEnv(),
        });
      } catch (err: any) {
        const reason = [err?.stderr, err?.stdout].filter(Boolean).join("\n").trim() || "Blocked by .claude/hooks/validate-bash.sh";
        return { block: true, reason };
      }
    }
  });

  pi.on("tool_execution_end", (event) => {
    if (!event.isError && (event.toolName === "edit" || event.toolName === "write")) {
      editedInRun = true;
      checkedInRun = false;
    }
  });

  pi.on("agent_before_settle", (event) => {
    if (!editedInRun || checkedInRun || event.outcome !== "completed") return;
    checkedInRun = true;
    const env = getDevEnv();
    try {
      verifyDiagnosticInvariants(env);
      execFileSync(
        "cmake",
        [
          "--build",
          "build/cmake",
          "--target",
          "tests",
          "builds_test-64",
          "plain-64",
          "desktop_test-64",
          "machine-64",
          "desktop_test-32",
          "machine-32",
          "desktop_test-16",
          "machine-16",
          "disasm_hex-64",
        ],
        { stdio: "pipe", encoding: "utf8", env }
      );
      execFileSync(
        "ctest",
        [
          "--test-dir",
          "build/cmake",
          "--output-on-failure",
          "-R",
          "^(instructions|kernel|fewer|builds|desktop-64|desktop-32|desktop-16)$",
        ],
        { stdio: "pipe", encoding: "utf8", env }
      );
      verifyDiagnosticInvariants(env);
      editedInRun = false;
    } catch (err: any) {
      const out = [err?.stdout, err?.stderr, err?.message].filter(Boolean).join("\n").trim() || String(err);
      return {
        entries: [
          ...event.entries,
          {
            type: "custom_message",
            customType: "agent-check",
            content: `agent-check failed after file edits:\n\n\`\`\`\n''${out}\n\`\`\``,
            display: true,
          },
        ],
        continue: true,
      };
    }
  });
}
EOF
              fi
              echo "hexaforth dev shell"
              echo "  cmake -S . -B build/cmake -DINSTALL_NEEDED=OFF"
              echo "  cmake --build build/cmake --target demo   # desktop on QEMU"
              echo "  cmake --build build/cmake --target run    # serial console on QEMU"
            '';
          };
        });

      apps = forAllSystems (system:
        let
          pkgs = import nixpkgs { inherit system; };
          gforthCompat = pkgs.writeShellScriptBin "gforth" ''
            exec ${pkgs.gforth}/bin/gforth -e "require libcc.fs also c-lib" "$@"
          '';
          includePath = pkgs.lib.makeSearchPathOutput "dev" "include" [
            pkgs.libtool
            pkgs.unicorn
            pkgs.gforth
          ];
          libPath = pkgs.lib.makeLibraryPath [
            pkgs.libtool.lib
            pkgs.unicorn
          ];
          runtimeDeps = [ gforthCompat ] ++ (with pkgs; [
            cmake
            gnumake
            gcc
            git
            curl
            libtool
            libtool.lib
            unicorn
            llvmPackages.clang-unwrapped
            lld
            llvm
            qemu
          ]);
          mkRunner = name: cmakeArgs: target:
            let
              drv = pkgs.writeShellApplication {
                inherit name;
                runtimeInputs = runtimeDeps;
                text = ''
                  export CMAKE_INCLUDE_PATH="${includePath}"
                  export CMAKE_LIBRARY_PATH="${libPath}"
                  export CPATH="${includePath}"
                  export LIBRARY_PATH="${libPath}"
                  cmake -S . -B build/cmake -DINSTALL_NEEDED=OFF ${cmakeArgs} "$@"
                  cmake --build build/cmake --target ${target}
                '';
              };
            in
            {
              type = "app";
              program = "${drv}/bin/${name}";
            };
        in
        rec {
          # Default: boots the desktop in QEMU (1 MIPS, 40 KB/s floppy, 2x scale)
          demo = mkRunner "hexaforth-demo" "-UMIPS -UDISK -USCALE -DKERNEL=hexaforth" "demo";
          # Unthrottled fast kernel in QEMU
          demo-fast = mkRunner "hexaforth-demo-fast" "-DMIPS= -DDISK= -DKERNEL=hexaforth-fast" "demo";
          # Serial console in QEMU (Ctrl-A X to exit)
          run = mkRunner "hexaforth-run" "" "run";
          default = demo;
        });
    };
}
