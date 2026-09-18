# opensweeper — App Store listing

Copy/paste into App Store Connect. Mirrors `android/play-assets/LISTING.md`, with
the differences Apple requires (subtitle, keywords, promotional text).
`scripts/asc_release.py listing` pushes the fields below and the screenshots,
so edit them here rather than in the console.

One rule that differs from Play:

- **Never mention Android, Google Play, or another platform** in the description.
  Apple rejects listings that reference competing stores.

## New App form (My Apps -> + -> New App)

| Field | Value |
| --- | --- |
| Platform | iOS |
| Name | `opensweeper` (must be unique App Store-wide; see fallbacks below) |
| Primary Language | English (U.S.) |
| Bundle ID | `com.danheskett.opensweeper` |
| SKU | `opensweeper` |
| User Access | Full Access |

If `opensweeper` is taken, in order of preference: `opensweeper Mines`,
`opensweeper Puzzle`, `opensweeper Game`. The name is public, capped at 30
characters, and can be changed with any later version — the SKU and bundle ID
cannot.

## Subtitle (<=30 chars)

```
Clear the field, flag mines
```

## Promotional text (<=170 chars)

Editable anytime without submitting a new build — use it for release notes or
seasonal copy.

```
No ads, no tracking, no accounts. The classic mine-sweeping puzzle, free and open source.
```

## Keywords (<=100 chars, comma-separated, no spaces after commas)

Do not repeat the app name — it is already indexed.

```
minesweeper,mines,puzzle,logic,classic,grid,flag,offline,brain,numbers
```

## Description (<=4000 chars)

```
The classic mine-sweeping puzzle. Reveal every cell that is not a mine. Each number tells you how many mines touch that cell; use them to work out where the mines are and flag them.

No ads. No tracking. No accounts. No in-app purchases. opensweeper never touches the network. You can play it in airplane mode.

HOW TO PLAY
• Tap a cell to reveal it. The first tap is always safe
• Press and hold to place a flag
• Tap a revealed number to clear its neighbours once its flags are placed
• Tap the face to start over

THREE DIFFICULTIES
• Beginner: 9 x 9, 10 mines
• Intermediate: 16 x 16, 40 mines
• Expert: 30 x 16, 99 mines
• Optional question-mark flags

FITS YOUR SCREEN
• Play upright or sideways, on iPhone or iPad
• Expert's wide grid stands upright on a phone held upright, so the cells stay as large as possible

FREE AND OPEN SOURCE
opensweeper is open source. Read the code, report a bug, or build it yourself: https://github.com/dannyheskett/opensweeper
```

## App Review notes

Sent to Apple's reviewer with every submission that has none yet
(`scripts/asc_release.py` sets them, with the team's review contact).

```
Thank you very much for reviewing my game. opensweeper is the classic mine-sweeping puzzle: tap a cell to reveal it, press and hold to place a flag, and tap a number to clear its neighbours. It needs no account, sign-in or network access. It runs on iPhone and iPad in either orientation; tapping the top of the screen opens the menu, where Options sets the difficulty.
```

## App information

- **Category (primary):** Games -> Puzzle
- **Category (secondary):** Games -> Board
- **Content Rights:** does not contain third-party content
- **Age Rating:** answer "None" to every question -> **4+**
- **Copyright:** `2026 Daniel Heskett`
- **Support URL:** https://danheskett.com
- **Marketing URL:** https://danheskett.com/projects/opensweeper/
- **Privacy Policy URL:** https://danheskett.com/app/privacy-policy/

## Screenshots

Two device families are required because the app declares iPhone and iPad
(`UIDeviceFamily [1,2]` in `ios/Info.plist`): the 6.9" iPhone set (1290x2796)
and the 13" iPad set (2064x2752). Each has an upright and a sideways folder;
upload one orientation per slot, not a mixture.

Captured from the web build -- the same C the app runs -- in a headless browser:

    npm i playwright-core
    make web
    node scripts/gen_store_screenshots.mjs --src build/web

## App Privacy (App Store Connect -> App Privacy)

Answer **"No, we do not collect data from this app."** — accurate and verified:
no network code, no analytics SDK, no permissions requested. This yields a
"Data Not Collected" privacy label. It has no API, so it is set once by hand,
and it must be **published** (the button at the top right) before a version can
be submitted.

## Pricing

Free. No in-app purchases.

## Export compliance

opensweeper uses no encryption of any kind. `ITSAppUsesNonExemptEncryption = false`
in `ios/Info.plist` stops App Store Connect asking on every upload.
