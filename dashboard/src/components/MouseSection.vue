<script setup>
import { settings, setSetting } from '../lib/store.js'

function formatMs(ms) {
  return (ms / 1000).toFixed(1) + 's'
}
</script>

<template>
  <section class="card">
    <h2>Mouse</h2>
    <div class="field">
      <label>
        Jiggle Duration
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
      <label>
        Move Size
        <span class="field-value">{{ settings.mouseAmp }}px</span>
      </label>
      <input
        type="range"
        :value="settings.mouseAmp"
        min="1" max="5" step="1"
        @input="setSetting('mouseAmp', Number($event.target.value))"
      />
    </div>
  </section>
</template>
