<script setup>
import { settings, setSetting } from '../lib/store.js'
import { MOUSE_STYLE_NAMES } from '../lib/protocol.js'

function formatMs(ms) {
  return (ms / 1000).toFixed(1) + 's'
}
</script>

<template>
  <section class="card">
    <h2>Mouse</h2>
    <div class="field">
      <label>
        Move Duration
        <span class="field-value">{{ formatMs(settings.mouseJig) }}</span>
      </label>
      <input
        type="range"
        :value="settings.mouseJig"
        min="500" max="90000" step="500"
        @input="setSetting('mouseJig', Number($event.target.value))"
      />
    </div>
    <div class="field">
      <label>
        Idle Duration
        <span class="field-value">{{ formatMs(settings.mouseIdle) }}</span>
      </label>
      <input
        type="range"
        :value="settings.mouseIdle"
        min="500" max="90000" step="500"
        @input="setSetting('mouseIdle', Number($event.target.value))"
      />
    </div>
    <div class="field">
      <label>Move Style</label>
      <select
        :value="settings.mouseStyle"
        @change="setSetting('mouseStyle', Number($event.target.value))"
      >
        <option v-for="(name, idx) in MOUSE_STYLE_NAMES" :key="idx" :value="idx">
          {{ name }}
        </option>
      </select>
    </div>
    <div class="field" :class="{ 'field-disabled': settings.mouseStyle === 0 }">
      <label>
        Move Size
        <span class="field-value">{{ settings.mouseStyle === 0 ? '---' : settings.mouseAmp + 'px' }}</span>
      </label>
      <input
        type="range"
        :value="settings.mouseAmp"
        min="1" max="5" step="1"
        :disabled="settings.mouseStyle === 0"
        @input="setSetting('mouseAmp', Number($event.target.value))"
      />
      <span v-if="settings.mouseStyle === 0" class="field-help">
        Bezier mode uses random sweep radius
      </span>
    </div>
  </section>
</template>

<style scoped>
.field-disabled {
  opacity: 0.4;
  pointer-events: none;
}
.field-help {
  display: block;
  font-size: 0.75rem;
  color: var(--text-secondary, #888);
  margin-top: 0.25rem;
}
</style>
