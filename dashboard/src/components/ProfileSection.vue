<script setup>
import { settings, setSetting } from '../lib/store.js'
</script>

<template>
  <section class="card">
    <h2>Profiles</h2>
    <p class="help-text">Adjust how much LAZY/BUSY profiles modify base timing.</p>
    <div class="field">
      <label>
        Lazy Adjustment
        <span class="field-value">{{ settings.lazyPct === 0 ? '0%' : '-' + settings.lazyPct + '%' }}</span>
      </label>
      <input
        type="range"
        :value="settings.lazyPct"
        min="0" max="50" step="5"
        @input="setSetting('lazyPct', Number($event.target.value))"
      />
    </div>
    <div class="field">
      <label>
        Busy Adjustment
        <span class="field-value">{{ settings.busyPct }}%</span>
      </label>
      <input
        type="range"
        :value="settings.busyPct"
        min="0" max="50" step="5"
        @input="setSetting('busyPct', Number($event.target.value))"
      />
    </div>
  </section>
</template>
