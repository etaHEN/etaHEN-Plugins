# Injector Plugin for etaHEN (PS5)
Plugins allows you to inject ELFs on apps automatically

## Features:

- **Auto-Injection**: Automatically detects game boot and injects payloads.
- **Title ID Support**: Specific payloads for specific games via `[CUSAXXXXX]` sections.
- **Execution Delays**: Custom sleep timers using the `!` command in config.

## :warning: Disclaimer

While we make every effort to deliver high quality products, we do not guarantee that our products are free from defects. Our software is provided **as is** and you use the software at your own risk.

# Quick Start

- Load [etaHEN](https://github.com/etaHEN/etaHEN) on your PS5.
- Load [NineS](https://github.com/buzzer-re/NineS) on your PS5.
- Enable plugin loading in your settings.
- Create the directory `/data/InjectorPlugin/` and place your `.elf` payloads there.
- Create your config file at `/data/etaHEN/Injector.ini`.
- Start this plugin from etaHEN Toolbox.

# Configuration

The plugin reads `/data/etaHEN/Injector.ini` to determine what to inject. 

### Syntax:
- `[default]` - Payloads in this section load for **every** game.
- `[CUSA12345]` - Payloads for a specific Title ID.
- `!5` - Wait for 5 seconds before the next action.
- `;` - Comment line.

### Example `Injector.ini`:

```ini
; Load plugins for any title

[default]
gamepad_helper.elf
!1


; Load plugins only for Playroom

[CUSA00001]
!1
no_share_watermark.elf
