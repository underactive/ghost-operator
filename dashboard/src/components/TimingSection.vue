<script setup>
import { settings, setSetting } from '../lib/store.js'

function formatMs(ms) {
  return (ms / 1000).toFixed(1) + 's'
}

function onKeyMinInput(e) {
  const val = Number(e.target.value)
  setSetting('keyMin', val)
  // Cross-constraint: min cannot exceed max
  if (val > settings.keyMax) {
    setSetting('keyMax', val)
  }
}

function onKeyMaxInput(e) {
  const val = Number(e.target.value)
  setSetting('keyMax', val)
  // Cross-constraint: max cannot be below min
  if (val < settings.keyMin) {
    setSetting('keyMin', val)
  }
}
</script>

<template>
  <section class="card">
    <h2>Keyboard Timing</h2>
    <div class="field">
      <label>
        Key Min Interval
        <span class="field-value">{{ formatMs(settings.keyMin) }}</span>
      </label>
      <input
        type="range"
        :value="settings.keyMin"
        min="500" max="30000" step="500"
        @input="onKeyMinInput"
      />
    </div>
    <div class="field">
      <label>
        Key Max Interval
        <span class="field-value">{{ formatMs(settings.keyMax) }}</span>
      </label>
      <input
        type="range"
        :value="settings.keyMax"
        min="500" max="30000" step="500"
        @input="onKeyMaxInput"
      />
    </div>
  </section>
</template>
