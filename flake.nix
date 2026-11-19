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
import type { ExtensionAPI } from "@earendil-works/pi-coding-agent";

export default function (pi: ExtensionAPI) {
  let editedInRun = false;
  let checkedInRun = false;

  pi.on("agent_start", () => {
    checkedInRun = false;
  });

  pi.on("tool_call", (event) => {
    if (event.toolName === "bash" && typeof (event as any).input?.command === "string") {
      try {
        execFileSync("./.claude/hooks/validate-bash.sh", [], {
          input: JSON.stringify({ tool_input: { command: (event as any).input.command } }),
          stdio: ["pipe", "pipe", "pipe"],
          encoding: "utf8",
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
    try {
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
        ],
        { stdio: "pipe", encoding: "utf8" }
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
        { stdio: "pipe", encoding: "utf8" }
      );
      editedInRun = false;
    } catch (err: any) {
      const out = [err?.stdout, err?.stderr].filter(Boolean).join("\n").trim() || String(err);
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
