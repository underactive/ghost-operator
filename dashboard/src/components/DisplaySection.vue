<script setup>
import { settings, setSetting } from '../lib/store.js'
import { ANIM_NAMES, SAVER_NAMES } from '../lib/protocol.js'
</script>

<template>
  <section class="card">
    <h2>Display</h2>
    <div class="field">
      <label>
        Brightness
        <span class="field-value">{{ settings.dispBright }}%</span>
      </label>
      <input
        type="range"
        :value="settings.dispBright"
        min="10" max="100" step="10"
        @input="setSetting('dispBright', Number($event.target.value))"
      />
    </div>
    <div class="field">
      <label>
        Screensaver Brightness
        <span class="field-value">{{ settings.saverBright }}%</span>
      </label>
      <input
        type="range"
        :value="settings.saverBright"
        min="10" max="100" step="10"
        @input="setSetting('saverBright', Number($event.target.value))"
      />
    </div>
    <div class="field">
      <label>Screensaver Timeout</label>
      <select
        :value="settings.saverTimeout"
        @change="setSetting('saverTimeout', Number($event.target.value))"
      >
        <option v-for="(name, idx) in SAVER_NAMES" :key="idx" :value="idx">
          {{ name }}
        </option>
      </select>
    </div>
    <div class="field">
      <label>Animation Style</label>
      <select
        :value="settings.animStyle"
        @change="setSetting('animStyle', Number($event.target.value))"
      >
        <option v-for="(name, idx) in ANIM_NAMES" :key="idx" :value="idx">
          {{ name }}
        </option>
      </select>
    </div>
  </section>
</template>
