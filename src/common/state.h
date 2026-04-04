#ifndef GHOST_COMMON_STATE_H
#define GHOST_COMMON_STATE_H

#include "config.h"
#include "sim_data.h"

// ============================================================================
// PORTABLE STATE — shared across all platforms
// Hardware-specific state lives in <platform>/state.h
// ============================================================================

// Display (dirty flag is platform-independent; display object is platform-specific)
extern volatile bool displayDirty;

// Settings & stats
extern Settings settings;
extern Stats stats;

// Mutable work mode array (initialized from const WORK_MODES[] + flash overrides)
extern WorkModeDef workModes[WMODE_COUNT];

// Profile
extern Profile currentProfile;
extern unsigned long profileDisplayStart;

// Encoder position (logical)
extern volatile int encoderPos;
extern int lastEncoderPos;

// Connection & enables
extern volatile bool deviceConnected;
extern bool usbConnected;
extern bool keyEnabled;
extern bool mouseEnabled;
extern uint8_t activeSlot;
extern uint8_t activeClickSlot;
extern uint8_t nextKeyIndex;

// Name editor state
extern uint8_t nameCharIndex[];
extern uint8_t activeNamePos;
extern bool    nameConfirming;
extern bool    nameRebootYes;
extern char    nameOriginal[];

// Decoy identity picker state
extern int8_t  decoyCursor;
extern int8_t  decoyScrollOffset;
extern bool    decoyConfirming;
extern bool    decoyRebootYes;
extern uint8_t decoyOriginal;

// Reset defaults confirmation state
extern bool    defaultsConfirming;
extern bool    defaultsConfirmYes;

// Reboot confirmation state
extern bool    rebootConfirming;
extern bool    rebootConfirmYes;

// Mode picker state (MODE_MODE sub-page)
extern uint8_t modePickerCursor;
extern bool    modePickerSnap;
extern bool    modeConfirming;
extern bool    modeRebootYes;
extern uint8_t modeOriginalValue;

// Generic carousel state (MODE_CAROUSEL)
extern uint8_t carouselCursor;
extern uint8_t carouselOriginal;
extern const CarouselConfig* carouselConfig;
extern CarouselCursorCallback carouselCallback;

// UI Mode
extern UIMode currentMode;
extern unsigned long lastModeActivity;
extern bool screensaverActive;
extern FooterMode footerMode;

// Menu state
extern int8_t   menuCursor;
extern int8_t   menuScrollOffset;
extern bool     menuEditing;
extern int8_t   helpScrollPos;
extern int8_t   helpScrollDir;
extern unsigned long helpScrollTimer;

// Timing
extern unsigned long startTime;
extern unsigned long lastKeyTime;
extern unsigned long lastMouseStateChange;
extern unsigned long lastMouseStep;
extern unsigned long lastDisplayUpdate;
extern unsigned long lastBatteryRead;

// Current targets (with randomness applied for mouse)
extern unsigned long currentKeyInterval;
extern unsigned long currentMouseJiggle;
extern unsigned long currentMouseIdle;

// Mouse state machine
extern volatile MouseState mouseState;
extern int8_t currentMouseDx;
extern int8_t currentMouseDy;
extern int32_t mouseNetX;
extern int32_t mouseNetY;
extern int32_t mouseReturnTotal;

// Scroll state
extern unsigned long lastScrollTime;
extern unsigned long nextScrollInterval;

// Easter egg
extern uint32_t mouseJiggleCount;
extern bool     easterEggActive;
extern uint8_t  easterEggFrame;

// Battery (platform sets these values)
extern int batteryPercent;
extern float batteryVoltage;
extern bool batteryCharging;

// LED timing
extern unsigned long ledKbOffMs;
extern unsigned long ledMouseOffMs;

// Serial status push (toggle with 't' command)
extern bool serialStatusPush;

// Schedule editor state
extern int8_t scheduleCursor;     // 0=Mode, 1=Start, 2=End
extern bool   scheduleEditing;    // true when adjusting selected value
extern uint8_t  scheduleOrigMode;   // snapshot for revert on timeout
extern uint16_t scheduleOrigStart;
extern uint16_t scheduleOrigEnd;

// Clock editor state (MODE_SET_CLOCK)
extern int8_t  clockCursor;     // 0=Hour, 1=Minute
extern bool    clockEditing;
extern uint8_t clockHour;       // 0-23
extern uint8_t clockMinute;     // 0-59

// Schedule / wall clock
extern bool timeSynced;
extern uint32_t wallClockDaySecs;
extern unsigned long wallClockSyncMs;
extern bool scheduleSleeping;
extern bool manualLightSleep;
extern bool scheduleManualWake;
extern unsigned long lastScheduleCheck;

// Button state
extern unsigned long funcBtnPressStart;
extern bool funcBtnWasPressed;
extern bool sleepPending;
extern bool lightSleepPending;
extern bool     sleepConfirmActive;
extern unsigned long sleepConfirmStart;
extern bool     sleepCancelActive;
extern unsigned long sleepCancelStart;

// Mute button state (D7)
extern unsigned long lastMuteBtnPress;

// Volume control state
extern int8_t        volFeedbackDir;      // 0=none, +1=up, -1=down
extern unsigned long volFeedbackStart;    // millis() when feedback started
extern bool          volMuted;            // mute toggle state (reset on boot)
extern bool          volPlaying;          // play/pause toggle state (reset on boot)

// D7 double-click state for Volume Control next/prev track
extern unsigned long volD7LastPress;
extern uint8_t       volD7ClickCount;     // 0 or 1 (waiting for 2nd click)

// K+M (form filling) sub-phase states
enum KbmsSubPhase { KBMS_MOUSE_SWIPE, KBMS_CLICK, KBMS_CLICK_PAUSE, KBMS_TYPING, KBMS_TYPING_PAUSE };

// M+K (drawing tool) sub-phase states
enum MskbSubPhase { MSKB_KEY_DOWN, MSKB_MOUSE_DRAW, MSKB_KEY_UP_PAUSE };

// Orchestrator state (simulation mode)
struct OrchestratorState {
  // Position in hierarchy
  uint8_t blockIdx;
  WorkModeId modeId;
  ActivityPhase phase;
  Profile autoProfile;

  // Timers (all millis-based, overflow-safe subtraction)
  unsigned long blockStartMs, blockDurationMs;
  unsigned long modeStartMs, modeDurationMs;
  unsigned long phaseStartMs, phaseDurationMs;
  unsigned long profileStintStartMs, profileStintMs;
  uint16_t switchGapMs;

  // Burst state (PHASE_TYPING sub-FSM)
  uint8_t burstKeysRemaining;
  bool keyDown;
  unsigned long keyDownMs;
  uint16_t currentKeyHoldMs;
  unsigned long nextKeyMs;
  bool inBurstGap;
  unsigned long burstGapEndMs;

  // Phantom click / window switch timers
  unsigned long lastPhantomClickMs;   // timestamp of last click (display flash)
  unsigned long nextWindowSwitchMs;

  // Lunch enforcement
  unsigned long dayStartMs;       // when day cycle started (block 0)
  bool lunchCompleted;            // has lunch happened this cycle?
  uint8_t lunchBlockIdx;          // cached lunch block index (0xFF = none)

  // Keystroke keepalive (activity floor)
  unsigned long lastSimKeystrokeMs;   // last keystroke sent by orchestrator
  uint8_t keepaliveBurstRemaining;    // keys left in current keepalive burst
  unsigned long keepaliveNextKeyMs;   // when to send next keepalive key

  // K+M (form filling) sub-phase state
  KbmsSubPhase kbmsSubPhase;
  uint8_t kbmsSwipesRemaining;
  uint8_t kbmsKeysRemaining;
  unsigned long kbmsSubPhaseStartMs;
  unsigned long kbmsSubPhaseDurMs;
  int8_t kbmsSwipeDx;
  int8_t kbmsSwipeDy;

  // K+M mouse step timer (replaces static local — reset on phase entry)
  unsigned long kbmsLastStepMs;

  // M+K (drawing tool) sub-phase state
  MskbSubPhase mskbSubPhase;
  uint8_t mskbStrokesRemaining;
  unsigned long mskbSubPhaseStartMs;
  unsigned long mskbSubPhaseDurMs;
  unsigned long mskbKeyHoldTarget;
  int8_t mskbStrokeDx;
  int8_t mskbStrokeDy;

  // M+K mouse step timer (replaces static local — reset on phase entry)
  unsigned long mskbLastStepMs;

  // Schedule preview overlay
  bool previewActive;
  unsigned long previewStartMs;

  // Scroll state for sim info rows (0=block, 1=mode, 2=profile)
  int8_t  scrollPos[3];
  int8_t  scrollDir[3];
  unsigned long scrollTimer[3];
};

extern OrchestratorState orch;

// Deferred settings save (avoids flash wear on rapid game-over)
extern bool settingsDirty;
extern unsigned long settingsDirtyMs;

// Lifetime stats periodic save
extern bool statsDirty;
extern unsigned long lastStatsSave;

#endif // GHOST_COMMON_STATE_H
