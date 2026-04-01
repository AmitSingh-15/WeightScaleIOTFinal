# 🛠️ BUILD & DEPLOYMENT CHECKLIST

## Step 0: Pre-Build Verification

- [ ] **Verify Arduino IDE Serial Device Found**
  - Plug in ESP32-P4 via USB
  - Tools → Port → Select your COM port
  - Expected: Something like "COM5" or "/dev/ttyUSB0"

- [ ] **Verify Board Selection**
  - Tools → Board → Select "ESP32 Dev Module" (or your exact board type)
  - Or use: Tools → Board Manager and search "ESP32"

- [ ] **Verify PSRAM Enabled**
  - Tools → PSRAM → **OPI PSRAM (Enabled)** ← CRITICAL!
  - (Without this, LVGL buffer allocation will fail)

- [ ] **Verify Partition Scheme**
  - Tools → Partition Scheme → Select one with at least 3MB APP space
  - Recommended: "Huge APP (3MB No OTA)" or "3MB APP / 1.5MB SPIFFS"

- [ ] **Verify All Source Files Present**
  - [x] `WeightScale_Base.ino` ← **MAIN FILE** (has setup/loop now)
  - [x] `main.cpp` ← (now just a stub - harmless)
  - [x] `lvgl_port.cpp` ← (FIXED: correct flush callback)
  - [x] All header files (*.h) present in same directory
  - [x] `config/app_config.h` exists
  - [x] All service files (scale_service_v2.cpp, storage_service.cpp, etc.)

---

## Step 1: Clean Build

- [ ] **Close Serial Monitor** (if open)
  
- [ ] **Delete Build Artifacts**
  - Sketch → Delete all build files
  - (Wait for Arduino to finish cleanup)

- [ ] **Verify Compilation**
  - Sketch → Verify/Compile
  - Expected output: `Leaving user provided CFLAGS:` and no errors
  - Watch for error messages - note any compilation failures

---

## Step 2: Verify Compilation Success

Check the last lines of the compiler output:

```
Compiling libraries...
Linking sketch...
The file size of ... is X bytes, which is Y% of the maximum.
Maximum code size is ZZZZZ bytes.
```

- [ ] **No red error messages** (yellow warnings are OK)
- [ ] **File size less than 75%** of maximum
  - If > 85%, you may have out-of-memory issues at runtime

---

## Step 3: Flash Firmware

- [ ] **Select Upload Speed**
  - Tools → Upload Speed → **921600** or **1500000**
  - (Faster is better for large binaries)

- [ ] **Plug in ESP32-P4 via USB**
  - Use a good quality USB cable (data transfer, not just power!)
  - Expected: Green LED on ESP32 board

- [ ] **Hit Upload Button**
  - Sketch → Upload
  - Watch for:
    ```
    Connecting........_____....._____
    ...
    Writing at 0x...
    ...
    Hard resetting via RTS pin
    ```

- [ ] **Wait for Upload Complete**
  - Expected: "Leave any other applications running COM port"
  - Do NOT unplug until upload shows "Hard resetting"

---

## Step 4: Verify Boot & Display Output

- [ ] **Open Serial Monitor**
  - Tools → Serial Monitor
  - Set baud rate to **115200**

- [ ] **Press ESP32 Reset Button** (or power cycle)
  - Look for boot output in Serial Monitor

- [ ] **Verify Boot Sequence**
  ```
  🚀 WEIGHINGSCALE ESP32-P4 BOOT START 🚀
  [BOOT] Step 1/6: Serial initialized ✓
  [BOOT] Step 2/6: Initializing Display & LVGL...
  [LVGL] ========================================
  [LVGL] Creating Board instance...
  [LVGL] ✓ Board instance created
  [LVGL] Calling board->init()...
  [LVGL] ✓ Board init() succeeded
  [LVGL] Calling board->begin()...
  [LVGL] ✓ Board begin() succeeded
  [LVGL] Getting LCD and Touch handles...
  [LVGL] ✓ LCD handle obtained
  [BOOT] Step 3/6: Initializing UI Styles...
  [BOOT] ✓ UI Styles ready ✓
  [BOOT] Step 4/6: Initializing Scale Service (HX711)...
  [BOOT] ✓ Scale Service ready ✓
  [BOOT] Step 5/6: Initializing Storage...
  [BOOT] ✓ Storage & Invoice ready ✓
  [BOOT] Step 6/6: Initializing Main UI & Screens...
  [BOOT] ✓ Main UI initialized ✓
  ✅ BOOT COMPLETE - SYSTEM READY
  Display is LIVE
  ```

- [ ] **PSRAM Allocation Line Present**
  - Search for: `[LVGL] Allocating PSRAM buffers: 196608 bytes`
  - Expected: **196608** (= 1024 × 96 × 2)
  - If different or missing: PSRAM not allocated correctly

- [ ] **No RED ❌ Error Messages**
  - All steps should show ✓ (checkmark) in green
  - If any shows ❌: Note the error and check USB connections

---

## Step 5: Verify Display Output

- [ ] **Look at Physical Display**
  - Expect to see home screen appear **within 2-3 seconds** of boot
  - Screen should show:
    - Device name at top
    - Weight value in center
    - Quantity field
    - SAVE / RESET buttons at bottom
  - No black screen, no garbage pixels

- [ ] **Test Touch Interaction**
  - Press "Settings" button → Admin password popup should appear
  - Type anything, press ENTER → "ACCESS DENIED" message
  - Type "1234", press ENTER → Should load settings screen

- [ ] **Test Weight Updates**
  - Place object on scale
  - Observe weight value updating on display in real-time
  - Max update rate: ~5 times per second (200ms minimum)

- [ ] **Test UI Navigation**
  - Settings → Back to Home ✓
  - History button → Load history screen ✓
  - Calibration buttons responsive ✓

---

## Step 6: Troubleshooting if Issues Occur

### ❌ Issue: Black Screen / No Display Output

**Check 1: PSRAM Enabled?**
```
Tools → PSRAM → MUST be "OPI PSRAM (Enabled)"
Then recompile and re-upload
```

**Check 2: Display Connections**
- Verify RGB cable fully seated on ESP32-P4 (if using RGB interface)
- Verify GT911 I2C on pins 19/20 (not other I2C pins)
- Check pin 5 for backlight control (should be HIGH at boot)

**Check 3: LVGL Initialization Failed?**
- Serial output shows: `[BOOT] ❌ FATAL: LVGL + Display init failed!`
- Check:
  - `board->init()` completed ✓
  - `board->begin()` completed ✓
  - LCD handle obtained ✓

**Check 4: PSRAM Buffer Allocation Failed?**
- Serial output shows: `[LVGL] ❌ FATAL: Buffer allocation failed`
- Solution:
  - Reduce LVGL buffer size (revert from 96 to 64 or 48 lines)
  - Or check available PSRAM with `esp_get_free_psram_size()`

---

### ❌ Issue: Display Glitchy / Checkerboard Pattern / Flashing

**Symptom**: Screen shows garbage pixels or flashing rainbow colors

**Solution**:
1. Verify RGB cable is fully seated
2. Try different USB cable (poor cable can cause voltage drops)
3. If persistent, try slower Upload Speed: Tools → Upload Speed → 115200

---

### ❌ Issue: Serial Output Shows Errors

**Error: `board->init() failed`**
- Display panel not detected
- Check connections and pin configuration in `app_config.h`

**Error: `LCD handle is NULL`**
- Display driver initialization failed
- Check if display is properly powered

**Error: `Buffer allocation failed`**
- PSRAM not available
- Tools → PSRAM → Ensure OPI PSRAM (Enabled)

---

### ❌ Issue: Compilation Errors

**Error: `undefined reference to 'home_screen_create'`**
- Missing `home_screen.cpp` or not compiling
- Check that ALL `.cpp` files are in the project directory

**Error: `'lvgl_port_init' was not declared`**
- Missing `#include "lvgl_port.h"` in WeightScale_Base.ino
- Verify include is present at line ~9

---

## Step 7: Success Criteria ✅

You've successfully fixed the black screen when:

1. ✅ Boot sequence completes with all ✓ checkmarks
2. ✅ Home screen appears within 2-3 seconds
3. ✅ Weight values update in real-time
4. ✅ Touch buttons responsive (Settings, History, etc.)
5. ✅ Admin password popup works (1234 = correct)
6. ✅ Screen transitions smooth (no flicker)
7. ✅ Serial Monitor shows no ❌ errors

---

## 📞 Next Steps

Once display is working:

1. **Verify Scale Functionality**
   - Calibrate with known weights
   - Test zero tare function
   - Verify different scale profiles (1KG, 100KG, 500KG)

2. **Test Invoice System**
   - Add items with SAVE button
   - Verify weight/quantity saved correctly
   - Test RESET and RESET_ALL functions
   - Check NVS storage persistence after power cycle

3. **Test WiFi & OTA** (if needed)
   - Verify WiFi list screen
   - Test OTA update mechanism

4. **Performance Tuning**
   - Monitor Serial output for any warnings
   - Check free heap: `esp_get_free_heap_size()`
   - Check free PSRAM: `esp_get_free_psram_size()`

---

## 🎉 Ready?

You're now ready to build! Start with **Step 0: Pre-Build Verification** and work through each step carefully.

Good luck! The display should now work perfectly. 🚀
