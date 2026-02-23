#include "sim_data.h"

// ============================================================================
// NAME ARRAYS
// ============================================================================

const char* OP_MODE_NAMES[]     = { "Simple", "Simulation" };
const char* JOB_SIM_NAMES[]     = { "Staff", "Developer", "Designer" };
const char* HEADER_DISP_NAMES[] = { "Job", "Device" };
const char* CLICK_TYPE_NAMES[] = { "Middle", "Left" };
const char* PHASE_NAMES[]       = { "TYPE", "MOUSE", "IDLE", "SWITCH" };

// ============================================================================
// WORK MODE DEFINITIONS (11 micro-modes)
// ============================================================================
// Timing per profile: { burstKeysMin/Max, interKeyMin/Max, burstGapMin/Max,
//                        keyHoldMin/Max, mouseDurMin/Max, idleDurMin/Max }

const WorkModeDef WORK_MODES[WMODE_COUNT] = {
  // --- EMAIL_COMPOSE (KB:65%) ---
  { WMODE_EMAIL_COMPOSE, "Email Compose", "Email", 65,
    { 25, 15, 60 },  // profile weights: LAZY 25%, NORMAL 15%, BUSY 60%
    {
      // LAZY
      { 5, 15, 180, 300, 8000, 30000, 120, 280, 8000, 20000, 3000, 12000 },
      // NORMAL
      { 15, 40, 100, 180, 3000, 12000, 80, 160, 5000, 15000, 2000, 8000 },
      // BUSY
      { 30, 80, 60, 120, 1000, 5000, 50, 110, 3000, 10000, 1000, 4000 },
    },
    60, 300, 15, 60
  },

  // --- EMAIL_READ (KB:20%) ---
  { WMODE_EMAIL_READ, "Email Read", "Read", 20,
    { 50, 35, 15 },
    {
      // LAZY
      { 1, 3, 200, 400, 15000, 40000, 120, 300, 10000, 25000, 5000, 15000 },
      // NORMAL
      { 2, 5, 150, 300, 8000, 25000, 80, 200, 8000, 20000, 3000, 10000 },
      // BUSY
      { 3, 8, 100, 200, 5000, 15000, 60, 150, 5000, 15000, 2000, 6000 },
    },
    30, 180, 20, 90
  },

  // --- PROGRAMMING (KB:80%) ---
  { WMODE_PROGRAMMING, "Programming", "Code", 80,
    { 20, 40, 40 },
    {
      // LAZY (thinking pauses)
      { 3, 10, 200, 350, 10000, 35000, 120, 280, 8000, 20000, 5000, 20000 },
      // NORMAL
      { 10, 40, 80, 160, 3000, 15000, 70, 150, 5000, 15000, 2000, 10000 },
      // BUSY (rapid coding)
      { 40, 100, 40, 100, 2000, 10000, 40, 100, 3000, 8000, 1000, 5000 },
    },
    120, 600, 20, 120
  },

  // --- BROWSING (KB:30%) ---
  { WMODE_BROWSING, "Browsing", "Browse", 30,
    { 30, 50, 20 },
    {
      // LAZY
      { 2, 8, 150, 300, 10000, 30000, 100, 250, 10000, 25000, 5000, 15000 },
      // NORMAL
      { 5, 15, 100, 200, 5000, 20000, 80, 180, 8000, 20000, 3000, 10000 },
      // BUSY
      { 8, 25, 70, 150, 3000, 12000, 50, 120, 5000, 12000, 2000, 6000 },
    },
    60, 300, 20, 90
  },

  // --- CHAT_SLACK (KB:60%) ---
  { WMODE_CHAT_SLACK, "Chat", "Chat", 60,
    { 20, 30, 50 },
    {
      // LAZY
      { 3, 10, 180, 300, 10000, 30000, 100, 250, 8000, 20000, 5000, 20000 },
      // NORMAL
      { 8, 25, 100, 180, 5000, 15000, 80, 160, 5000, 15000, 3000, 12000 },
      // BUSY
      { 15, 50, 50, 120, 2000, 8000, 50, 110, 3000, 8000, 1000, 5000 },
    },
    30, 180, 15, 60
  },

  // --- VIDEO_CONF (KB:10%) ---
  { WMODE_VIDEO_CONF, "Video Conference", "Meeting", 10,
    { 70, 25, 5 },
    {
      // LAZY
      { 1, 2, 300, 500, 30000, 90000, 150, 400, 15000, 40000, 10000, 30000 },
      // NORMAL
      { 1, 3, 200, 400, 20000, 60000, 100, 300, 10000, 30000, 8000, 20000 },
      // BUSY
      { 2, 5, 150, 300, 10000, 30000, 80, 200, 8000, 20000, 5000, 12000 },
    },
    300, 900, 60, 300
  },

  // --- DOC_EDITING (KB:55%) ---
  { WMODE_DOC_EDITING, "Doc Editing", "Docs", 55,
    { 20, 35, 45 },
    {
      // LAZY
      { 5, 15, 180, 300, 8000, 25000, 100, 250, 8000, 20000, 3000, 12000 },
      // NORMAL
      { 10, 30, 100, 180, 4000, 15000, 80, 160, 6000, 18000, 2000, 8000 },
      // BUSY
      { 20, 60, 60, 130, 2000, 8000, 50, 120, 4000, 12000, 1000, 5000 },
    },
    90, 480, 20, 90
  },

  // --- COFFEE_BREAK (KB:15%) ---
  { WMODE_COFFEE_BREAK, "Coffee Break", "Coffee", 15,
    { 80, 15, 5 },
    {
      // LAZY
      { 1, 3, 300, 500, 30000, 60000, 150, 400, 15000, 40000, 15000, 45000 },
      // NORMAL
      { 1, 5, 200, 400, 20000, 45000, 100, 300, 10000, 30000, 10000, 30000 },
      // BUSY
      { 2, 5, 150, 300, 15000, 30000, 80, 200, 8000, 20000, 8000, 20000 },
    },
    900, 1200, 120, 300
  },

  // --- LUNCH_BREAK (KB:5%) ---
  { WMODE_LUNCH_BREAK, "Lunch Break", "Lunch", 5,
    { 90, 10, 0 },
    {
      // LAZY
      { 1, 2, 300, 500, 60000, 120000U, 150, 400, 20000, 60000, 30000, 90000 },
      // NORMAL
      { 1, 3, 200, 400, 30000, 90000, 100, 300, 15000, 45000, 20000, 60000 },
      // BUSY (weight 0, but define anyway for safety)
      { 1, 3, 200, 400, 30000, 90000, 100, 300, 15000, 45000, 20000, 60000 },
    },
    1800, 3600, 300, 600
  },

  // --- IRL_MEETING (KB:10%) ---
  { WMODE_IRL_MEETING, "IRL Meeting", "IRL Mtg", 10,
    { 80, 15, 5 },
    {
      // LAZY
      { 1, 3, 300, 500, 30000, 90000, 150, 400, 15000, 40000, 15000, 45000 },
      // NORMAL
      { 2, 8, 200, 350, 15000, 45000, 100, 250, 10000, 30000, 10000, 30000 },
      // BUSY
      { 3, 10, 150, 250, 10000, 30000, 80, 180, 8000, 20000, 5000, 15000 },
    },
    1800, 3600, 300, 600
  },

  // --- FILE_MGMT (KB:25%) --- (design tool proxy)
  { WMODE_FILE_MGMT, "File Management", "Files", 25,
    { 15, 50, 35 },
    {
      // LAZY
      { 2, 5, 200, 350, 10000, 30000, 100, 250, 10000, 25000, 5000, 15000 },
      // NORMAL
      { 3, 10, 120, 220, 5000, 18000, 80, 180, 8000, 20000, 3000, 10000 },
      // BUSY
      { 5, 15, 80, 160, 3000, 10000, 60, 140, 5000, 12000, 2000, 6000 },
    },
    60, 300, 20, 90
  },
};

// ============================================================================
// DAY TEMPLATES (3 job simulations)
// ============================================================================

const DayTemplate DAY_TEMPLATES[JOB_SIM_COUNT] = {
  // === STAFF (office worker) ===
  { "Staff", 9, {
    { "AM Email",    0,   30, 3, {{ WMODE_EMAIL_COMPOSE, 40 }, { WMODE_EMAIL_READ, 40 }, { WMODE_CHAT_SLACK, 20 }} },
    { "AM Work",     30,  90, 5, {{ WMODE_DOC_EDITING, 35 }, { WMODE_BROWSING, 25 }, { WMODE_EMAIL_COMPOSE, 20 }, { WMODE_FILE_MGMT, 10 }, { WMODE_CHAT_SLACK, 10 }} },
    { "Coffee",      120, 15, 1, {{ WMODE_COFFEE_BREAK, 100 }} },
    { "Late AM",     135, 75, 5, {{ WMODE_DOC_EDITING, 30 }, { WMODE_EMAIL_READ, 20 }, { WMODE_BROWSING, 20 }, { WMODE_CHAT_SLACK, 15 }, { WMODE_FILE_MGMT, 15 }} },
    { "Lunch",       210, 45, 1, {{ WMODE_LUNCH_BREAK, 100 }} },
    { "PM Meeting",  255, 45, 2, {{ WMODE_VIDEO_CONF, 60 }, { WMODE_IRL_MEETING, 40 }} },
    { "PM Work",     300, 90, 5, {{ WMODE_DOC_EDITING, 35 }, { WMODE_EMAIL_COMPOSE, 25 }, { WMODE_BROWSING, 20 }, { WMODE_FILE_MGMT, 10 }, { WMODE_CHAT_SLACK, 10 }} },
    { "Coffee 2",    390, 15, 1, {{ WMODE_COFFEE_BREAK, 100 }} },
    { "Wind Down",   405, 75, 4, {{ WMODE_EMAIL_READ, 30 }, { WMODE_EMAIL_COMPOSE, 25 }, { WMODE_BROWSING, 25 }, { WMODE_CHAT_SLACK, 20 }} },
  }},

  // === DEVELOPER ===
  { "Developer", 9, {
    { "Standup",     0,   15, 2, {{ WMODE_CHAT_SLACK, 60 }, { WMODE_VIDEO_CONF, 40 }} },
    { "Email",       15,  15, 3, {{ WMODE_EMAIL_READ, 60 }, { WMODE_EMAIL_COMPOSE, 30 }, { WMODE_CHAT_SLACK, 10 }} },
    { "Deep Work 1", 30,  120, 4, {{ WMODE_PROGRAMMING, 70 }, { WMODE_BROWSING, 15 }, { WMODE_FILE_MGMT, 10 }, { WMODE_CHAT_SLACK, 5 }} },
    { "Coffee",      150, 15, 2, {{ WMODE_COFFEE_BREAK, 80 }, { WMODE_BROWSING, 20 }} },
    { "Deep Work 2", 165, 90, 3, {{ WMODE_PROGRAMMING, 75 }, { WMODE_BROWSING, 15 }, { WMODE_FILE_MGMT, 10 }} },
    { "Lunch",       255, 45, 1, {{ WMODE_LUNCH_BREAK, 100 }} },
    { "Code Review", 300, 60, 4, {{ WMODE_PROGRAMMING, 40 }, { WMODE_BROWSING, 30 }, { WMODE_CHAT_SLACK, 20 }, { WMODE_DOC_EDITING, 10 }} },
    { "Deep Work 3", 360, 75, 4, {{ WMODE_PROGRAMMING, 70 }, { WMODE_BROWSING, 15 }, { WMODE_FILE_MGMT, 10 }, { WMODE_CHAT_SLACK, 5 }} },
    { "Wind Down",   435, 45, 4, {{ WMODE_EMAIL_COMPOSE, 30 }, { WMODE_CHAT_SLACK, 30 }, { WMODE_BROWSING, 20 }, { WMODE_DOC_EDITING, 20 }} },
  }},

  // === DESIGNER ===
  { "Designer", 9, {
    { "AM Email",    0,   20, 3, {{ WMODE_EMAIL_READ, 50 }, { WMODE_EMAIL_COMPOSE, 30 }, { WMODE_CHAT_SLACK, 20 }} },
    { "Design 1",    20,  100, 4, {{ WMODE_FILE_MGMT, 40 }, { WMODE_BROWSING, 35 }, { WMODE_DOC_EDITING, 15 }, { WMODE_CHAT_SLACK, 10 }} },
    { "Coffee",      120, 15, 2, {{ WMODE_COFFEE_BREAK, 80 }, { WMODE_BROWSING, 20 }} },
    { "Review",      135, 45, 4, {{ WMODE_VIDEO_CONF, 40 }, { WMODE_BROWSING, 30 }, { WMODE_CHAT_SLACK, 20 }, { WMODE_FILE_MGMT, 10 }} },
    { "References",  180, 30, 3, {{ WMODE_BROWSING, 70 }, { WMODE_FILE_MGMT, 20 }, { WMODE_CHAT_SLACK, 10 }} },
    { "Lunch",       210, 45, 1, {{ WMODE_LUNCH_BREAK, 100 }} },
    { "Design 2",    255, 120, 4, {{ WMODE_FILE_MGMT, 45 }, { WMODE_BROWSING, 30 }, { WMODE_DOC_EDITING, 15 }, { WMODE_CHAT_SLACK, 10 }} },
    { "Feedback",    375, 30, 4, {{ WMODE_CHAT_SLACK, 40 }, { WMODE_EMAIL_COMPOSE, 30 }, { WMODE_EMAIL_READ, 20 }, { WMODE_VIDEO_CONF, 10 }} },
    { "Wind Down",   405, 75, 4, {{ WMODE_EMAIL_COMPOSE, 25 }, { WMODE_BROWSING, 30 }, { WMODE_FILE_MGMT, 25 }, { WMODE_CHAT_SLACK, 20 }} },
  }},
};
