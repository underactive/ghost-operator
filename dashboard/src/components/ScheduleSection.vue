<script setup>
import { settings, status, setSetting } from '../lib/store.js'
import { SCHEDULE_MODE_NAMES, formatTime5 } from '../lib/protocol.js'
</script>

<template>
  <section class="card">
    <h2>Schedule</h2>
    <div class="field">
      <label>Mode</label>
      <select
        :value="settings.schedMode"
        @change="setSetting('schedMode', Number($event.target.value))"
      >
        <option v-for="(name, idx) in SCHEDULE_MODE_NAMES" :key="idx" :value="idx">
          {{ name }}
        </option>
      </select>
      <p class="help-text" v-if="settings.schedMode === 1">
        Device enters deep sleep at end time. Press button to wake.
      </p>
      <p class="help-text" v-else-if="settings.schedMode === 2">
        Device enters light sleep at end time and auto-wakes at start time.
      </p>
    </div>
    <div class="field" v-if="settings.schedMode !== 0">
      <label>
        Start Time
        <span class="field-value">{{ formatTime5(settings.schedStart) }}</span>
      </label>
      <input
        type="range"
        :value="settings.schedStart"
        min="0" max="287" step="1"
        @input="setSetting('schedStart', Number($event.target.value))"
      />
    </div>
    <div class="field" v-if="settings.schedMode !== 0">
      <label>
        End Time
        <span class="field-value">{{ formatTime5(settings.schedEnd) }}</span>
      </label>
      <input
        type="range"
        :value="settings.schedEnd"
        min="0" max="287" step="1"
        @input="setSetting('schedEnd', Number($event.target.value))"
      />
    </div>
    <div class="field schedule-status" v-if="settings.schedMode !== 0">
      <span class="sync-indicator" :class="{ synced: status.timeSynced }">
        {{ status.timeSynced ? 'Time synced' : 'Not synced' }}
      </span>
      <span v-if="status.schedSleeping" class="sleep-badge">Sleeping</span>
    </div>
  </section>
</template>

<style scoped>
.schedule-status {
  display: flex;
  align-items: center;
  gap: 0.75rem;
  margin-top: 0.25rem;
}

.sync-indicator {
  font-size: 0.8rem;
  color: var(--text-dim);
}

.sync-indicator.synced {
  color: #66bb6a;
}

.sleep-badge {
  font-size: 0.75rem;
  padding: 0.15rem 0.5rem;
  border-radius: 4px;
  background: rgba(255, 167, 38, 0.15);
  color: var(--warning);
  font-weight: 600;
}
</style>
