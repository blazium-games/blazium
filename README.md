# Blazium Engine

<p align="center">
  <a href="https://blazium.app">
    <img src="logo_outlined.svg" width="400" alt="Blazium Engine logo">
  </a>
</p>

## 2D and 3D cross-platform game engine

**[Blazium Engine](https://blazium.app) is a free, open-source game engine for 2D and 3D games and applications.** It is a fork of the [Godot Engine](https://godotengine.org), first committed in [October 2024](https://github.com/blazium-games/blazium/commit/e86a91030a60be7d37e99c7a6069d75181d5138c). It is released under the [MIT license](https://mit-license.org). There is no royalty, no seat fee, and no revenue threshold. A game made with Blazium, including the engine code shipped with it, can be used under that license.

There are two engine lines. They are not interchangeable. A project opened on one line is not the same project as on the other.

- [`blazium-dev`](https://github.com/blazium-games/blazium/tree/blazium-dev) is the Godot 4.3+ line. `version.py` is Godot 4.3.2, status `stable`, docs channel `4.3`. Use this for projects that stay on the Godot 4.3 base.
- [`blazium_4.8`](https://github.com/blazium-games/blazium/tree/blazium_4.8) is the Godot 4.8+ line. `version.py` is Godot 4.8.0, status `dev`, docs channel `latest`. Hub, the crash reporter, and the skills and subagents track this line. The skills API baseline 0.8.x applies here, not on `blazium-dev`.

Both lines still report the external product version `0.6.0-dev`. The last published release write-up is [0.6.725](https://blazium.app/changelog?v=release_0.6.725) (2026-07-01). That page is the notes, not a claim that 0.6.725 is the newest build, and not a claim that it belongs to only one line. Download the current build from the [download page](https://blazium.app/download).

## What Blazium adds

On top of the Godot editor it is based on, Blazium ships first-party modules and editor tools. Grouped, from the [0.6.725 notes](https://blazium.app/changelog?v=release_0.6.725):

- **Editor and agents:** an MCP server in the editor (JustAMCP), the Autowork testing framework, and real-time multiuser editing.
- **Stores and platforms:** a first-party Steam module, the Discord Social SDK, and Microsoft GDK export.
- **Data:** CSV, ENV, and INI, SQLite, JWT, and arbitrary-precision numbers.
- **Content and gameplay:** a Tiled importer, goal-oriented action planning (GOAP), and experimental Luau.
- **Live operations:** Twitch, Kick, OBS, IRC, RCON, Socket.IO, and an HTTP server.
- **Older consoles:** PS1, PS2, N64, and Interactive DVD export, used with the toolchain. `ps3` and `ps4` are reserved and do not ship.

## Godot, Redot, and Blazium

Godot, Redot, and Blazium are MIT engines in the Godot family.

[Godot](https://godotengine.org) is the upstream project. [Redot](https://github.com/Redot-Engine/redot-engine/blob/master/README.md) was forked in September 2024. Its current line is the 26.x LTS, based on Godot 4.5. It stays close to upstream and adds selected language and editor features, including GDScript structs, traits, and nullable static types in the 26.3 beta.

Blazium’s difference inside that family is the modules above, plus the desktop and agent toolchain, and bridges to stores and consoles. This page does not claim that Redot lacks a feature that was not checked, and it does not claim that Blazium has Redot’s 26.3 language features.

## Unity, Unreal, and Blazium

Unity, Unreal, and Blazium are different engines. They are not forks of each other. The comparison here is license and workflow. It is not a claim of market share, and it is not a claim that Blazium matches Nanite, Lumen, or Unity’s mobile reach.

- **Unity** (Unity Technologies) is a C# editor. The editor is closed. Use is by seat subscription. The runtime fee was canceled on 12 September 2024 and does not apply to Unity 6 or earlier. Distributing the runtime has no royalty when the seat terms are met ([pricing update](https://unity.com/products/pricing-updates), [editor terms](https://unity.com/legal/editor-terms-of-service/software)). Source access is a paid plan feature, and modifications stay inside Unity’s license. Unity does not give you an MIT engine you can fork and ship.
- **Unreal** (Epic) uses C++ and Blueprints. Source is available under the EULA, and Epic keeps the engine IP. Games pay a 5% royalty on worldwide gross revenue after $1 million lifetime per product. Revenue from the Epic Games Store is royalty-free. Non-game commercial use over that revenue line is a seat license of $1,850 per seat per year ([license](https://www.unrealengine.com/license)).
- **Blazium** is MIT. No royalty, no seat fee, no revenue threshold. Scripting is GDScript first, with C# through the Mono module, C++ modules, and experimental Luau. The practical difference from Unity and Unreal is that license plus the first-party modules, Hub, CLI, toolchain, and agent skills.

## Blazium Engine

[Blazium Engine](https://blazium.app) is the free MIT game engine. The editor and the tools in the table below belong to it. `blazium-cli` installs editors, manages projects, remote-controls a running editor, and deploys to Steam and itch.io. Two lines ship: `blazium-dev` is the Godot 4.3+ line, and `blazium_4.8` is the Godot 4.8+ line. Hub, the crash reporter, skills, and subagents track `blazium_4.8`.

## Blazium Games

[Blazium Games](https://blazium.games) is a separate store, operated by Divine Games, Inc. It is the platform for playing and publishing games, applications, mods, and assets. A game on that store does not have to be made with the Blazium engine, and the engine does not require that store. `chauffeur` is the only upload tool for Blazium Games. Docs are at [docs.blazium.games](https://docs.blazium.games).

The Godot and Blazium editor AssetLib can list packages from `https://api.blazium.online/api/v1/public/asset-library`. The store has a Made with Blazium shelf. Those are the bridges. They do not make the engine and the store the same product.

## Community

- Official website: [https://blazium.app/](https://blazium.app/)
- IndieDB blog: [https://www.indiedb.com/engines/blazium-engine](https://www.indiedb.com/engines/blazium-engine)
- Official community: [Discord](https://blazium.app/chat)
- Docs: [docs.blazium.app](https://docs.blazium.app)

## Ecosystem

| Product | Role | Release |
|---------|------|---------|
| [Engine](https://github.com/blazium-games/blazium) | The editor. Two lines: `blazium-dev` (Godot 4.3+) and `blazium_4.8` (Godot 4.8+). Hub, crash reporter, skills, and subagents track `blazium_4.8`. | [blazium.app/download](https://blazium.app/download) |
| [CLI](https://github.com/blazium-games/blazium-cli) | Install editors, projects, remote control, Steam and itch.io deploy. Not the Games uploader. | Linux and Windows, x86_64 and x86_32. Catalog: [cli.json](https://cdn.blazium.app/cli/cli.json) |
| [Hub](https://github.com/blazium-games/blazium-hub) | Desktop companion; installers bundle the CLI. Engine builds track `blazium_4.8`. | Linux and Windows, x86_64 and x86_32. |
| [Crash reporter](https://github.com/blazium-games/blazium_crash_reporter) | Sidecar UI for engine and Hub crash reports. Engine builds track `blazium_4.8`. | Linux and Windows, x86_64 and x86_32. Catalog: [crash_reporter.json](https://cdn.blazium.app/crash_reporter/crash_reporter.json) |
| [Toolchain](https://github.com/blazium-games/blazium-toolchain) | PS1, PS2, N64, and Interactive DVD. `ps3` and `ps4` are reserved and do not ship. | Linux and Windows, x86_64 and x86_32. Catalog: [toolchain.json](https://cdn.blazium.app/toolchain/toolchain.json) |
| [Skills](https://github.com/blazium-games/blazium-skills) | Agent skill packs for Claude, Cursor, Codex, and Grok. Own semver, separate from the 0.8.x API baseline. | Catalog: [skills.json](https://cdn.blazium.app/skills/skills.json) |
| [Subagents](https://github.com/blazium-games/blazium-subagents) | Studio roster that loads those skills. Own semver. | Catalog: [subagents.json](https://cdn.blazium.app/subagents/subagents.json) |
| [Blazium Games](https://blazium.games) | Separate store. Upload with chauffeur, not with these tools. | Site [blazium.games](https://blazium.games), docs [docs.blazium.games](https://docs.blazium.games). |

## Getting the engine

Official binaries are on the [download page](https://blazium.app/download).

[Compilation instructions](https://docs.blazium.app/contributing/development/compiling) cover every supported platform.

To contribute, or to report a bug, start with the [contributing guide](CONTRIBUTING.md). The [class reference](https://docs.blazium.app/classes/index.html) is also available inside the editor. Docs are maintained in [blazium-docs](https://github.com/blazium-games/blazium-docs).

![Alt](https://repobeats.axiom.co/api/embed/1b819dee6b40c3805707313803df99f2ad12cb37.svg "Repobeats analytics image")
[![DigitalOcean Referral Badge](https://web-platforms.sfo2.cdn.digitaloceanspaces.com/WWW/Badge%201.svg)](https://www.digitalocean.com/?refcode=c9796b8f52e2&utm_campaign=Referral_Invite&utm_medium=Referral_Program&utm_source=badge)
