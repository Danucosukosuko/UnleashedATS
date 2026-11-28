# User Manual

## Main screen

![](_static/screenshot-main.png)

* **RSSI meter** (top left corner), also serves as a mono/stereo indicator in FM mode (one/two rows). The single mark ahead of the bar is the peak hold, which stays for a moment and then slides back down.
* **Settings save icon** (right after the RSSI meter). The settings are saved to non-volatile memory after 10 seconds of inactivity.
* **Bluetooth icon** (right after the save icon). Different colors indicate the connection status.
* **Wi-Fi icon** (top right area near the battery). Different colors indicate the connection status.
* **Battery status** (top right corner). It doesn't show the voltage when charged, see [#36](https://github.com/esp32-si4732/ats-mini/issues/36#issuecomment-2778356143). The only indication that the battery is charging is the hardware LED on the bottom of the receiver, which turns ON during charging.
* **Band name and modulation** (VHF & FM, top center). The rounded **A** or **B** indicator after the mode shows the active VFO slot. See the [Bands table](#bands-table) for more details.
* **Info panel** (the box on the left side), also **Menu**. The parameters are explained in the [Menu](#menu) section. It can also show the current time, or the date and time after a successful synchronization. When Off Timer is enabled, this row alternates with the remaining time as **Zzz: hh:mm**. If the clock is not set, only the countdown is shown.
* **Frequency** (center of the screen).
* **FM station name** (RDS PS) or **frequency name** (right below the frequency). A frequency name appears for some popular frequencies like FT8, SSTV, CB channels, or a shortwave [schedule](#schedule). Can also display current **menu option** using a bigger font when the Zoom Menu setting is enabled.
* **Tuning scale** (bottom of the screen). Can be replaced with additional RDS fields (RT, PTY) when extended RDS is enabled, or RSSI/SNR graphs in Scan mode.

## Alternative UI

![](_static/screenshot-smeter.png)

The differences are:

* **Stereo indicator** is on the right side of the VFO indicator.
* **Tuning scale** (right under the station name). Numbers on the left & right sides are the band limits.
* **S/N Meter** (in dB). The range is 0...127 and the visual indicator linearly displays this range.
* **RSSI & S-Meter** (the number is in dBµV, the meter is in S-points). Please note that the RSSI range is also 0...127 (no negative values) and according to [these tables](https://dl4zao.de/_downloads/Dezibel.pdf) any values below S4 on HF (rssi < 4) and below S7 on VHF (rssi < 2) are bogus. Thus it is very far from being precise, and also depends on the antenna impedance.

Both meters can be replaced with additional RDS fields (RT, PTY) when extended RDS is enabled, or RSSI/SNR graphs in Scan mode.

## Controls

Controls are implemented through the encoder knob:

| Gesture                | Result                                                                |
|------------------------|-----------------------------------------------------------------------|
| Rotate                 | Tunes frequency, navigates menu, adjusts parameters.                  |
| Click (<0.5 sec)       | Opens menu, selects.                                                  |
| Short press (>0.5 sec) | Volume shortcut in VFO mode, context-dependent action in other modes. |
| Long press (>2 sec)    | Sleep on/off.                                                         |
| Press and rotate       | Direct frequency input mode, fine tuning in Seek mode.                |

### Direct frequency input mode

* Press and rotate the encoder to select the step (digit or "half-digit").
* Rotate the encoder to adjust the frequency.
* Use short press to align frequency to the current step.
* To exit the mode, click the encoder or wait for a couple of seconds.

## Menu

The menu can be invoked by clicking the encoder button and is closed automatically after a couple of seconds.

* **Mode** - FM (only available on the VHF band); LSB, USB, AM (available on other bands). The receiver doesn't support the NFM mode (on any band, including the CB) due to limitations of the SI4732 chip.
* **Band** - List of [Bands](#bands-table).
* **Volume** - 0 (silent) ... 63 (max). The headphone volume level can be low (compared to the built-in speaker) due to limitation of the initial hardware design. Use short press to mute/unmute.
* **VFO A/B** - Click to switch between two tuning slots, or short press (>0.5 sec) to copy the active slot into the inactive slot without switching. Both actions return to tuning. Each slot remembers its frequency (including SSB fine tuning), band, mode, step, and bandwidth. Tuning and memory recall affect only the active slot; other settings and memory slots are shared. Both slots and the active selection are saved after 10 seconds of inactivity. Initially, both slots contain the current tuning position.
* **Step** - Tuning step (not every step is available on every band and mode).
* **Seek** - Seek up or down on AM/FM, normal tuning on LSB/USB (hardware seek function is not supported by SI4732 on SSB). Rotate or click the encoder to stop the seek. Use short press to switch between the seek, [schedule](#schedule) and auto modes. Use press and rotate for manual fine tuning. In the auto mode (marked "Auto"), rotating the encoder erases every memory slot and fills it again from a sweep of the current band.
* **Scan** - Scan a frequency range and plot the RSSI (S) and SNR (N) graphs (unfortunately, these metrics are almost meaningless in SSB modes due to SI4732 patch limitations). Both graphs are normalized to 0.0 - 1.0 range. While the Scan mode is active, short press the encoder for 0.5 seconds to rescan. To abort a running scan process click or rotate the encoder.
* **Memory** - 99 slots to store favorite frequencies. Named slots show their name instead of the frequency and mode in the menu. A matching name also appears below the tuned frequency; a received RDS station name takes priority. Short press (>0.5 sec) on an empty slot to store the current frequency, short press to erase a slot, switch between stored slots by rotating the encoder, click to exit the menu. Edit memory slots using the receiver's **Memory** page over [Wi-Fi](#wi-fi) or via [remote control](remote.md). For older firmware, use the [legacy USB memory editor](memory.md).
* **Mem Scan** - Hop through the occupied memory slots, passing over the quiet ones and staying on an active channel as long as it carries a signal. Click to stop on the current channel, rotate to stop and take over the tuning.
* **Auto Store** - Sweep the current band and store the stations found into the memory slots, named after their RDS name. **Add New** fills the free slots, **Replace** drops the stations of the current band first, **Back** leaves without sweeping. FM and AM only. Click to cut a sweep short.
* **Stations** - Browse the FM station list downloaded with *Settings → Load Stations*. Pick the country, then the province or state, then rotate through its stations to tune them; the title shows the municipality of the highlighted station, and long names slide along so they can be read in full. The province you pick becomes your home province, marked with `*`: when there is no RDS, only the stations of your province name the frequency you tune. Short press on the country deletes the list. **Back** goes up one level.
* **RDS AF** - Alternative frequencies of the station, the one being received marked with `>`. Click a frequency to tune it; the encoder then moves between them, tuning as it goes, until you click again. **Back** leaves the menu.
* **RDS EON** - Other networks announced by the station (Enhanced Other Networks), by name or PI code. Click one to see its frequencies, then click a frequency to tune it as in **RDS AF**.
* **PTY Search** - Go to the next FM station with a given programme type, such as News, Rock Music or Sport. Rotate to pick the type (the RDS or the US RBDS list, as set in *Settings → RDS*) and click to search. The audio fades out while the receiver goes up the band from where it is, reading the programme type of every station it can hear; it stops on the first one that matches, or comes back to where it was after going all the way round. Click to stop the search and go back, rotate to stop it where it is.
* **DX Watch** - Watch the FM band for tropospheric and sporadic E openings while you are not listening. It warns that it may drain the battery, as the receiver stays busy, and recommends USB power; **Start** begins, **Back** leaves. The audio fades out and the display dims while it sweeps 87.5-108 MHz in 100 kHz steps, showing every channel as it goes. The first three sweeps learn the place: every channel with any signal on it, whether its RDS could be read or not, and the program identification (PI) of every station. After that only the channels that were empty are checked, and a catch needs the same new PI in two clean RDS groups in a row, so local stations, their spill onto neighbouring channels and stations listed for your home province in the downloaded station list never set it off. A catch is reported once: the display lights up, the station plays and the catch is logged, with its RDS name, to `dxlog.csv`, which the web interface serves at `/dxlog.csv`. Click to stop, rotate to stop and take over the tuning; a catch left alone for a minute goes back to watching. Each catch is logged with its UTC date and time when the clock is set.

  **Log** shows the catches on the receiver, newest first: frequency, PI, name and the UTC date and time. Click one to tune it. **Clear log** at the end empties the log after a second click. The [web updater](https://danucosukosuko.github.io/UnleashedATS/update/) reads the log over USB and saves it as CSV, or in the URDS format FMLIST uses for RDS logs, ready to check with the URDS Log Validator.
* **Squelch** - mute the speaker when the selected RSSI (dBuV) or SNR (dB) level is lower than the defined threshold. The setting is saved separately for each mode (FM, LSB, USB, AM). When Off, short press the encoder button to switch between RSSI and SNR. When enabled, short press turns squelch Off. Unlikely to work in SSB mode.
* **Bandwidth** - Selects the bandwidth of the channel filter.
* **AGC/ATTN** - Automatic Gain Control (on/off) or Attenuation level. The attenuator is not applicable to SSB mode.
* **AVC** - Sets the maximum gain for automatic volume control (not applicable to FM mode). In LSB/USB, short press toggles Hold to disable AVC and hold the current gain. Rotate to resume automatic AVC and adjust the maximum gain. Hold is temporary and clears on band/mode reinitialization or restart; the displayed maximum gain is not a measurement of held gain.
* **SoftMute** - Sets softmute max attenuation (only applicable to AM/SSB).
* **Settings** - Settings submenu.

## Settings menu

* **Brightness** - Display brightness level (10...255). The minimal one draws about 80mA of the battery power, the default one about 100mA, the max level about 120mA.
* **Date/Time** - Set the UTC date and time with the encoder. Click to select the next field, or short press to set the clock and close the menu. A timeout closes the menu without changing the clock.
* **UTC Offset** - Affects the displayed date and time. Please note that automatic DST transitions are not supported; the offset needs to be adjusted manually.
* **Sleep Timer** - Automatic sleep interval in seconds (Off - disabled).
* **Sleep Mode** - Locked - lock the encoder rotation during sleep; Unlocked - allow tuning the frequency in sleep mode; CPU Sleep - the maximum power saving mode. With the display being on, default brightness, and Wi-Fi the power consumption is about 170mA, without Wi-Fi 100mA, Locked/Unlocked modes draw about 70mA, CPU sleep mode draws about 40mA.
* **Off Timer** - Short press the menu entry to shut down immediately into deep sleep, or click to adjust the timer (5–240 minutes, in 5-minute steps). While adjusting, turn the encoder to start or restart the timer; short press to disable it (Off). The remaining time alternates with the date and time in the info panel. The timer works during sleep and restarts on each power-up. After shutdown, cycle the power switch or press reset to restart the receiver. This is not a true power-off state: the receiver still draws some current.
* **Theme** - Color theme. Themes made in the web [theme editor](#theme-editor) are listed after the built-in ones, and built-in themes hidden there are left out.
* **UI Layout** - Alternative UI layouts. For now there is just one alternative UI with large S-meter and S/N-meter.
* **Zoom Menu** - Display the currently selected menu item using a larger font (accessibility option).
* **Scroll Dir.** - Menu scroll direction for clockwise encoder turn.
* **Enc. Accel.** - Increase tuning and adjustment speed when turning the encoder quickly (On by default). Disable for a constant step per click.
* **RDS** - Radio Data System options: PS - radio station name, CT - date and time, RT - text, PTY - genre, ALL (EU/US) - everything, OFF - turn RDS off, ALL+AF (EU/US) - everything, and follow the station to a stronger alternative frequency of the same programme when the reception fades. The TP, AF and EON labels next to the frequency show which of these services the station carries. When the station tags its RadioText with RT+ (RadioText Plus), the RadioText line shows the artist and title of what is playing. RDS CT should contain UTC date and time, but some stations incorrectly transmit local or completely bogus values. The clock is synchronized from RDS only once. To synchronize it again, disable and re-enable RDS CT or switch the receiver off and on.
* **FM Stereo** - **Auto** lets the receiver blend down to mono on its own as the signal gets worse, **Mono** forces mono audio, trading the stereo image for less hiss on a weak station.
* **DX Mode** - Weak signal listening. **On** turns off the soft mute that lowers the volume of weak stations, makes the audio blend to mono earlier on FM, and lowers the seek, Auto Store and Mem Scan thresholds, on FM, MW, LW and SW.
* **FM Region** - FM de-emphasis time constant by region (50µs for EU/JP/AU and 75µs for the US).
* **Capacitor** - Automatically adjusts the antenna tuning capacitor on MW1, MW2, MW3, and 160 m bands. Off by default; only works on receivers without a Hi-Z input circuit.
* **Calibration** - SSB calibration offset (-2000...2000, per mode/band).
* **DSP Patches** - Select **Default** or an uploaded AM/SSB patch set. Manage these experimental patches on the web interface's **Patches** page.
* **Load EiBi** - download the EiBi [schedule](#schedule) (requires Wi-Fi internet connection).
* **Load Stations** - download the FM station list of a country (requires Wi-Fi internet connection and a set clock). Pick the region, then the country; countries already on the receiver are marked with `*`. A new country replaces the previous one. Listed stations are named on screen when they send no RDS.
* **Update FW** - Firmware updates over Wi-Fi (requires an internet connection). See [updating over Wi-Fi](flash.md#update-over-wi-fi).
  * **Check** shows the version available.
  * **Update** opens the notes of a newer release: rotate to read them, then pick **Install** or **Cancel** at the end. A click while reading jumps to the choices.
  * **Channel** chooses between **Stable**, releases only, and **Beta**, which also offers the betas published before a release. Betas get the new features first and may still have rough edges.
  * **Previous FW** starts the firmware kept from before the last update, after a second click to confirm. The receiver keeps two firmware slots, and an update always goes to the one not in use, so the version before it stays there until the next update. Going back does not change the settings or memories.
  * **Recovery** restarts into the [recovery app](#recovery-app).
* **USB Port** - USB serial mode: Off (default), [Ad hoc](remote.md#ad-hoc-protocol), or [CAT TS-480](remote.md#kenwood-cat-protocol). The selected protocol accepts [remote control](remote.md) commands over the USB serial port.
* **TCP Port** - TCP control mode: Off (default), [Ad hoc](remote.md#ad-hoc-protocol), or [CAT TS-480](remote.md#kenwood-cat-protocol). One client can use the selected [remote control](remote.md#tcp-over-wi-fi) protocol over Wi-Fi on port 60000. Wi-Fi must be enabled separately.
* **Bluetooth** - Bluetooth LE mode: Off (default), [Ad hoc](remote.md#ad-hoc-protocol), [CAT TS-480](remote.md#kenwood-cat-protocol), HID, or Unpair All. Ad hoc and CAT TS-480 expose the selected [remote control](remote.md) protocol over BLE. HID makes the receiver act as a BLE HID central and connect to supported Bluetooth remotes/keyboards so their buttons can control tuning and menu actions. Unpair All clears all saved Bluetooth bonds and then switches Bluetooth to Off. WARNING: it is not recommended to enable both Bluetooth and Wi-Fi at the same time (the receiver might become unstable).
* **Wi-Fi** - Wi-Fi mode: Off (default), Access Point, Access Point + Connect, Connect, Sync Only. More details on that below.
* **Battery** - Show the battery as a voltage or as a percentage. The percentage follows the discharge curve of a lithium cell from the filtered voltage; while USB power is connected the receiver shows the charging symbol instead, as it cannot measure the cell then.
* **About** - Informational screens (Help, Authors, System).

## Wi-Fi

There are a couple of modes (2.4GHz only):

* **Off** (default)
* **AP Only** - Access Point mode. The receiver creates its own access point called `ATS-Mini` and starts the web server on <http://10.1.1.1>.
* **AP+Connect** - Access Point mode + try to connect to one of the three configured access points. If the connection succeeds, the receiver will synchronize the time every 4 hours and start the web server on both <http://10.1.1.1> and a dynamic IP address it got from the configured access point.
* **Connect** - try to connect to one of the three configured access points, start the web server on a dynamic IP, then synchronize the time every 4 hours.
* **Sync Only** - connect to synchronize the clock, then disable Wi-Fi. Wi-Fi is also disabled if connection or time synchronization times out.

### Initial configuration

1. Enable the **AP Only** mode (the receiver will briefly display its 10.1.1.1 IP address).
2. Connect to the `ATS-Mini` access point from your phone or computer. There is no internet connection available on this access point. When connecting from a phone, it might be necessary to switch off the mobile data connection and any VPN/firewall software.
3. Open a browser and visit the following URL: <http://10.1.1.1>. The status web page should open. Alternatively, you can try the mDNS address <atsmini.local> in your browser.
4. Click the `Wi-Fi` link, enter the network name and password for your access point, and press `Save`.
5. Switch the Wi-Fi mode to **AP+Connect** or **Connect** (the receiver will briefly show its new dynamic IP address it got from a configured access point).
6. Connect your phone/computer to the same access point and open the receiver’s new IP address or <atsmini.local> in your browser.

From now on you can switch the modes as you want and connect to your receiver either via the `ATS-Mini` internal access point (if enabled, mostly useful when there are no access points around), or via an external access point and a dynamic IP address. The receiver will always listen on the mDNS address <atsmini.local>.

```{hint}
When on the go, you can set up a mobile Wi-Fi hotspot on your smartphone and use it to connect the receiver to the internet.
```

### Web pages and settings

The `Status` page shows the network addresses, firmware version, date/time, band, frequency and mode, signal strength, signal-to-noise ratio, and battery voltage. Click `Download screenshot` to save an image of the receiver's display.

The `Memory` page lets you edit slot names, bands, frequencies (in Hz), and modes. Use the up/down buttons to reorder slots, and press `Save` to apply changes to the receiver. `Export` downloads the current table as JSON; `Import` loads a previously exported file, including files from the legacy USB editor.

The `Config` page lets you set the UTC date/time manually or from the browser, and adjust display, radio, power, hardware, and USB/TCP/Bluetooth modes. See the [Settings menu](#settings-menu) for descriptions of shared settings.

* `Off Timer`: changing the duration restarts its countdown; saving the same duration leaves it running.
* `Half-step Encoder`: leave it off for the original EC11 20C20P encoder (20 clicks, 20 pulses), and enable it for an EC11 30C15P encoder (30 clicks, 15 pulses). Half-step mode is also required for the LILYGO T-Embed SI4732 hardware variant. See [discussion #87](https://github.com/esp32-si4732/ats-mini/discussions/87) for more information.
* `Audio Clamp`: disable it for receivers without an audio clamp circuit to suppress clicks when muting or changing bands or modes. Leave it enabled for receivers with the circuit and for LILYGO T-Embed SI4732.
* `Splash Screen`: upload or delete an optional [splash image](_static/splash-outdoor.png) shown when the receiver starts.

Wi-Fi and Bluetooth LE TX power can be changed only on the `Config` page, under Power. The defaults are 17 dBm for Wi-Fi and 0 dBm for Bluetooth LE. Changes apply immediately and are saved across restarts. Lower power reduces range and may disconnect remote controls or the web interface.

The `Wi-Fi` page lets you select the Wi-Fi mode, configure up to three access points the receiver will try to connect to and add optional login and password to protect the memory, settings, Wi-Fi, and firmware update pages. Enable `Scan Hidden SSIDs` only if one of the configured access points does not broadcast its network name; leaving it off makes Wi-Fi connection faster. Changing Wi-Fi mode may disconnect the browser; Off and Sync Only disable the web interface.

The `Remote` page (`/remote`) is an FM-DX style control panel: frequency, signal history, RDS PS, PI, PTY and RadioText, the AF and EON lists, band buttons, seek and step, presets kept in the browser and an RDS logger. It uses the same login as the other pages. To listen to the audio over the network as well, use the [PC companion](https://github.com/Danucosukosuko/UnleashedATS-Webserver).

Firmware updates are available through **Settings → Update FW** or the web interface's **Update** page, which also shows the notes of the release found and lets you change the update channel. See [updating over Wi-Fi](flash.md#update-over-wi-fi) for instructions.

### Theme editor

The `Themes` page (`/themes`) lists the built-in and custom themes, each with a few of its colors.

* **Use** switches the receiver to that theme.
* **Copy** opens the editor on a copy of a theme, **Edit** opens a custom theme itself. Every color of the screen can be changed, grouped by the part of the screen it paints. With **Live preview on the receiver** on, the receiver shows each change as you make it, without saving it.
* **Save** stores the changes to the custom theme being edited, **Save as new** stores them as a new custom theme. Up to eight custom themes can be kept.
* **Delete** removes a custom theme. Built-in themes cannot be deleted, but **Hide** takes them off the list in *Settings → Theme*, and **Show** puts them back.

Custom themes are kept with the settings, and survive firmware updates.

On the **Patches** page, upload and select experimental AM/SSB patches (see [discussion #377](https://github.com/esp32-si4732/ats-mini/discussions/377)):

1. Find the section for **Custom 1**, **Custom 2**, or **Custom 3** where you want to upload the patches.
2. Select **AM** or **SSB**, choose the matching `.bin` patch file (up to 32 KiB), and click **Save**. Repeat for the other mode if needed.
3. Activate the patch set on the web page or through **Settings → DSP Patches** on the receiver. Only sets with uploaded patches are selectable.

Missing modes use Default; FM is unaffected. To replace patches in the active set, activate another set or **Default** first.

To remove a patch set, check **Delete set** and click **Save**. Deletion takes priority over any selected upload.

## Schedule

The receiver can download the [EiBi](http://eibispace.de/dx/eibi.txt) shortwave schedule and use it to display broadcasting stations, allowing you to quickly tune to them. Here’s how it works:

* The schedule only needs to be downloaded once via [Wi-Fi](#wi-fi). It will be stored in the receiver's flash memory so it doesn't need to be fetched every time the device powers on.
* To display scheduled stations correctly, the receiver’s clock must be set. You can set the UTC date/time from the Settings menu or the configuration web page, or configure a Wi-Fi internet connection and use Sync Only mode for NTP synchronization. The UTC offset setting doesn’t affect the schedule. A less reliable alternative is to use RDS CT, but this requires finding a station that broadcasts UTC time (not local time).
* Once set up, the receiver will display station names currently broadcasting on specific frequencies (only scheduled times are considered; days of the week are ignored for now).
* You can quickly jump between stations using the Seek mode (marked by a clock icon). To switch between modes, short press the encoder while in Seek mode.

## Recovery app

UnleashedATS installs a small recovery app in a flash partition of its own, which firmware updates never touch, so it is there whatever state the firmware is in. To start it, hold the encoder while switching the receiver on until the amber *RECOVERY* screen appears (about two seconds). It also starts by itself when no firmware on the receiver can start, and when the firmware crashes three times in a row before getting to run. *Settings → Update FW → Recovery* and the safe mode restart into it as well.

Rotate to choose and click to run; options that erase something ask for a second click.

* **Start the firmware** - Start the firmware selected for booting.
* **Slot 1**, **Slot 2** - The two firmware slots, with the version in each and *(boots)* next to the one selected. Click one to make it the one that boots and start it.
* **Install from browser** - Shows how to install a firmware file from a phone or a computer: join the `ATS-Mini Recovery` Wi-Fi network (no password), open `http://10.1.1.1` and upload the `-ota.bin` or `ats-mini.ino.bin` file of your variant. Click to go back.
* **Install latest** - Download and install the latest release over one of the Wi-Fi networks saved in the firmware.
* **Reset settings**, **Erase memories**, **Forget Wi-Fi**, **Erase files**, **Factory reset** - The same as in the [safe mode](#safe-mode).
* **USB flashing** - Restart into the ESP32 download mode for esptool or a web flasher, with a screen saying so.

The recovery app also has a **web page** of its own, available as soon as it starts: on the `ATS-Mini Recovery` network at `http://10.1.1.1`, and on your own network at the address shown at the top of the screen when it connects to one of the saved networks. It asks for the login of the web interface when one is set. The page shows both firmware slots and starts either of them, uploads and installs a firmware file with a progress bar, installs the latest release, erases each kind of data, and restarts into USB flashing mode.

A firmware is always installed into the slot that does not boot, so the one there before stays as the way back.

The recovery app comes with a [USB flash](flash.md) of the full firmware; Wi-Fi updates keep the receiver as it is. It is not available on the LilyGO T-Embed, where the encoder button is the ESP32 boot strapping pin.

## Safe mode

Hold the encoder while switching the receiver on, and release it when the *Safe mode* screen appears. With the recovery app installed, release it within two seconds, before the recovery app takes over; the safe mode is also where the receiver goes after crashing three times in a row at startup when there is no recovery app. Rotate to choose an option and click to run it; the options that erase something ask for a second click, and rotating cancels. The line at the bottom says what the selected option does.

* **Continue** - Start the receiver normally.
* **Reset settings** - Back to the default settings, bands and debug options. Memories, station lists, the DX Watch log, Wi-Fi networks and custom themes stay.
* **Erase memories** - Clear the memory slots.
* **Forget Wi-Fi** - Clear the saved networks and the login of the web interface.
* **Erase files** - Erase the station lists, the DX Watch log, the splash image and the uploaded patches.
* **Factory reset** - Erase everything, as on a new receiver. This is what holding the encoder at power on did before.
* **Previous firmware** - Start the firmware kept from before the last update. Its version is shown next to it, or *none kept* when there is none.
* **Recovery app** - Restart into the [recovery app](#recovery-app), when it is installed.
* **Recovery Wi-Fi** - Start the receiver with Wi-Fi on for this session, both the `ATS-Mini` access point and the saved networks. Open `http://10.1.1.1/update` (or the address the receiver gets on your network) to install a firmware file from the browser.
* **Reinstall latest** - Start the receiver and install the latest release again as soon as it is online over a saved network.
* **USB flashing** - Restart into the ESP32 download mode, so that esptool or a web flasher can write the firmware without holding the BOOT button. Switch the receiver off and on to leave it.
* **Restart** - Restart the receiver.

A new firmware is also kept on probation until it has started: if an update does not get as far as the main screen, the next reset brings the previous firmware back by itself.


## Bands table

| Name | Min frequency | Max frequency | Default mode |
|------|---------------|---------------|--------------|
| VHF  | 64 MHz        | 108 MHz       | FM           |
| ALL  | 150 kHz       | 30000 kHz     | AM           |
| 11M  | 25600 kHz     | 26100 kHz     | AM           |
| 13M  | 21500 kHz     | 21900 kHz     | AM           |
| 15M  | 18900 kHz     | 19100 kHz     | AM           |
| 16M  | 17400 kHz     | 18100 kHz     | AM           |
| 19M  | 15100 kHz     | 15900 kHz     | AM           |
| 22M  | 13500 kHz     | 13900 kHz     | AM           |
| 25M  | 11000 kHz     | 13000 kHz     | AM           |
| 31M  | 9000 kHz      | 11000 kHz     | AM           |
| 41M  | 7000 kHz      | 9000 kHz      | AM           |
| 49M  | 5000 kHz      | 7000 kHz      | AM           |
| 60M  | 4000 kHz      | 5100 kHz      | AM           |
| 75M  | 3500 kHz      | 4000 kHz      | AM           |
| 90M  | 3000 kHz      | 3500 kHz      | AM           |
| MW3  | 1700 kHz      | 3500 kHz      | AM           |
| MW2  | 495 kHz       | 1701 kHz      | AM           |
| MW1  | 150 kHz       | 1800 kHz      | AM           |
| 160M | 1800 kHz      | 2000 kHz      | LSB          |
| 80M  | 3500 kHz      | 4000 kHz      | LSB          |
| 40M  | 7000 kHz      | 7300 kHz      | LSB          |
| 30M  | 10000 kHz     | 10200 kHz     | LSB          |
| 20M  | 14000 kHz     | 14400 kHz     | USB          |
| 17M  | 18000 kHz     | 18200 kHz     | USB          |
| 15M  | 21000 kHz     | 21500 kHz     | USB          |
| 12M  | 24800 kHz     | 25000 kHz     | USB          |
| 10M  | 28000 kHz     | 29700 kHz     | USB          |
| CB   | 25000 kHz     | 28000 kHz     | AM           |

## Remote control

Various remote control options are documented on the dedicated [Remote control](remote.md) page.
