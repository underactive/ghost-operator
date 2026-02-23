<script setup>
import { settings, setSetting } from '../lib/store.js'
import {
  JOB_SIM_NAMES, CLICK_TYPE_NAMES, SWITCH_KEYS_NAMES, HEADER_DISP_NAMES,
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
      <label>Click Type</label>
      <select
        :value="settings.clickType"
        @change="setSetting('clickType', Number($event.target.value))"
      >
        <option v-for="(name, idx) in CLICK_TYPE_NAMES" :key="idx" :value="idx">
          {{ name }}
        </option>
      </select>
      <p class="help-text">Middle click is safest (no accidental window actions). Left click is more realistic.</p>
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
