<script setup>
import { settings, setSetting } from '../lib/store.js'
import {
  JOB_SIM_NAMES, CLICK_TYPE_NAMES, SWITCH_KEYS_NAMES, HEADER_DISP_NAMES,
  formatTime5, formatShiftDuration,
} from '../lib/protocol.js'
</script>

<template>
  <section class="card">
    <h2>Simulation</h2>

    <div class="field">
      <label>Job Type</label>
      <select
        :value="settings.jobSim"
        @change="setSetting('jobSim', Number($event.target.value))"
      >
        <option v-for="(name, idx) in JOB_SIM_NAMES" :key="idx" :value="idx">
          {{ name }}
        </option>
      </select>
      <p class="help-text">Day schedule template — determines work block timing and mode weights.</p>
    </div>

    <div class="field">
      <label>
        Job Start Time
        <span class="field-value">{{ formatTime5(settings.jobStart) }}</span>
      </label>
      <input
        type="range"
        min="0"
        max="287"
        :value="settings.jobStart"
        @input="setSetting('jobStart', Number($event.target.value))"
      />
      <p class="help-text">Time your workday begins — simulation aligns work blocks to this time.</p>
    </div>

    <div class="field">
      <label>
        Shift Duration
        <span class="field-value">{{ formatShiftDuration(settings.shiftDur) }}</span>
      </label>
      <input
        type="range"
        min="240"
        max="720"
        step="30"
        :value="settings.shiftDur"
        @input="setSetting('shiftDur', Number($event.target.value))"
      />
      <p class="help-text">Work duration (excluding lunch). Total day = shift + lunch.</p>
    </div>

    <div class="field" v-if="settings.shiftDur >= 300">
      <label>
        Lunch Duration
        <span class="field-value">{{ settings.lunchDur }}m</span>
      </label>
      <input
        type="range"
        min="15"
        max="120"
        step="5"
        :value="settings.lunchDur"
        @input="setSetting('lunchDur', Number($event.target.value))"
      />
      <p class="help-text">Break duration at the 4-hour mark.</p>
    </div>

    <div class="field" v-if="settings.shiftDur < 300">
      <p class="help-text">No lunch break for shifts under 5 hours.</p>
    </div>

    <div class="field">
      <label>
        Job Performance
        <span class="field-value">{{ settings.jobPerf }}</span>
      </label>
      <input
        type="range"
        min="0"
        max="11"
        :value="settings.jobPerf"
        @input="setSetting('jobPerf', Number($event.target.value))"
      />
      <p class="help-text">Activity intensity (0 = near-idle, 5 = baseline, 11 = maximum). Also adjustable via encoder on OLED.</p>
    </div>

    <div class="field">
      <label>Window Switching</label>
      <select
        :value="settings.winSwitch"
        @change="setSetting('winSwitch', Number($event.target.value))"
      >
        <option :value="0">Off</option>
        <option :value="1">On</option>
      </select>
    </div>

    <div class="field" v-if="settings.winSwitch === 1">
      <label>Switch Keys</label>
      <select
        :value="settings.switchKeys"
        @change="setSetting('switchKeys', Number($event.target.value))"
      >
        <option v-for="(name, idx) in SWITCH_KEYS_NAMES" :key="idx" :value="idx">
          {{ name }}
        </option>
      </select>
      <p class="help-text">Alt-Tab for Windows/Linux, Cmd-Tab for macOS.</p>
    </div>

    <div class="field">
      <label>Auto Clicks</label>
      <select
        :value="settings.phantom"
        @change="setSetting('phantom', Number($event.target.value))"
      >
        <option :value="0">Off</option>
        <option :value="1">On</option>
      </select>
    </div>

    <div class="field" v-if="settings.phantom === 1">
      <label>Click Button</label>
      <select
        :value="settings.clickType"
        @change="setSetting('clickType', Number($event.target.value))"
      >
        <option v-for="(name, idx) in CLICK_TYPE_NAMES" :key="idx" :value="idx">
          {{ name }}
        </option>
      </select>
      <p class="help-text">Middle click is safest. Right click triggers context menus. Button 4/5 are browser back/forward.</p>
    </div>

    <div class="field">
      <label>Header Display</label>
      <select
        :value="settings.headerDisp"
        @change="setSetting('headerDisp', Number($event.target.value))"
      >
        <option v-for="(name, idx) in HEADER_DISP_NAMES" :key="idx" :value="idx">
          {{ name }}
        </option>
      </select>
      <p class="help-text">What to show in the OLED header — current job simulation name or device name.</p>
    </div>
  </section>
</template>
