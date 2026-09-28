# Contributing to MathLens

Thank you for helping. This guide covers how to get set up, what we expect from a change, and how to write commits.

## Getting started

1. Fork the repository and create a branch from `main`: `git checkout -b feature/short-name`.
2. Build and test the engine:
   ```bash
   cmake -S mathengine -B mathengine/build -DCMAKE_BUILD_TYPE=Release
   cmake --build mathengine/build -j && ./mathengine/build/mathtests
   ```
3. Build the app: `cd android && ./gradlew assembleDebug`, then run it on an emulator or a device.

For larger changes (a new solver, a new screen, a new dependency), please open an issue first so we can agree on the
approach.

## What a good change looks like

- **Engine (`mathengine/`)**
  - C++23, no exceptions (use `std::expected` / `std::optional`), no third-party dependencies.
  - Every new capability or fix comes with a case in `mathengine/tests/tests.cpp`.
  - Answers must be verified where possible (substitute roots back, check integrals numerically).
  - Explanations are written from the solution's own data: never state something the solver did not compute.
- **App (`android/`)**
  - Kotlin and Jetpack Compose. Follow the existing gallery-minimal design: warm paper tones, ink, sparing terracotta, serif headings.
  - Test on a device or emulator; include before/after screenshots for visual changes in the pull request.
  - Text shown to users is plain and friendly: no internal syntax such as `;`, no jargon without explanation.
- **Licensing**
  - Start every new source file with the SPDX header used in its folder:
    - `mathengine/`: `SPDX-License-Identifier: GPL-3.0-or-later`
    - `android/` and `tools/`: `SPDX-License-Identifier: MIT`
  - Only add dependencies, models or assets under permissive licenses (MIT, BSD, Apache 2.0, OFL for fonts), and record them in `THIRD_PARTY_NOTICES.md` and on the in-app Licenses screen.
  - Contribute only code you wrote yourself or are entitled to license under these terms. By submitting a pull request, you agree that your contribution is licensed under the license of the folder it touches.

## Commit messages

Commits use the form:

```
[type][module] Short summary in the imperative mood

Optional body: what changed and why.
```

**Types**

| Type | Use for |
|---|---|
| `feature` | new capability |
| `bugfix` | something that was wrong now works |
| `improvement` | better behaviour, performance or UX of something existing |
| `cleanup` | refactoring, removals, formatting; no behaviour change |
| `docs` | documentation only |
| `build` | build system, dependencies, CI |
| `test` | tests only |
| `license` | licensing and notices |

**Modules**: `core` (engine core: numbers, expressions, simplifier), `cpp` (engine solvers and API),
`ocr` (recognition and camera), `ui` (screens and components), `explain` (animations), `graph`, `wallpaper`,
`android` (app plumbing), `tools`, `repo`.

Examples:

```
[feature][cpp] Add eigenvalues for large matrices with QR iteration
[bugfix][ocr] Keep all equations of a system when one line is misread
[improvement][graph] Draw curves at a constant pen speed
```

## Pull requests

- Keep each pull request focused on one change.
- Describe the problem, the approach, and how you tested it.
- Make sure `mathtests` passes and the app builds.

## Code of conduct

Be kind and constructive. Critique ideas, not people. Assume good intent.
