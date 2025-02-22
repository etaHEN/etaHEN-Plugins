etaHEN Plugin SDK
=====
- The etaHEN Plugin SDK is a collection of tool open source tools and samples for developing for [etaHEN](https://github.com/LightningMods/etaHEN), requires the [Johns SDK](https://github.com/ps5-payload-dev/sdk)
- The etaHEN Plugin SDK has support for dynamic linking with libraries available in the libs folder 
- Any ELF or Plugin made with this SDK is already jailbroken, no code required

> [!Note]
> Kstuff slows down plugin loading!
> If you are on a firmware that uses kstuff it may take up to 1 min for the plugin to load 
>

ELFs vs etaHEN Plugins
-------------
- **ELFs**: are meant for single use payload-like programs where they run a single task (like showing hwinfo in a notification, etc) will need johns elf losder active 
- **Plugins**: are daemons that are meant to run the whole time the console is on in the background (can only be loaded by etaHEN)


Plugins
--------
- Plugins can be loaded by etaHEN automatically from either `/mnt/usb<number>/etaHEN/plugins` or internally from `/data/etaHEN/plugins` when etaHEN is first ran or by the etaHEN toolbox 
- Plugins located in the `etahen > plugins` folder on USB root are given priority over internally installed plugins in `/data/etaHEN/plugins`
- Duplicate Plugins are ignored by etaHEN on startup (but are listed in the toolbox's plugin section), etaHEN also checks if the plugin title id is already running
- Plugins can be killed and ran via etaHEN's toolbox
- each plugin has its own title id and version defined in the plugins `CMakeLists.txt` file as follows
```
set(PLUGIN_TITLE_ID "TEST00000")
set(PLUGIN_VERSION "9.99")
```
- libhijacker provides plugins with tools to modify processes

```c
static void default_handler(int sig) {
    (void) sig;
    kill(getpid(), SIGKILL);
}
```


Credits
-------

* If you have a list of people who helped with everything to get this far, add it. Otherwise, you know who you are.
