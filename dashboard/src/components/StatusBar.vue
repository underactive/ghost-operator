<script setup>
import { ref, onMounted } from 'vue'
import { status, settings, setSetting } from '../lib/store.js'
import {
  PROFILE_NAMES, MODE_NAMES, MOUSE_STATE_NAMES,
  OP_MODE_NAMES, WORK_MODE_NAMES, PHASE_NAMES,
} from '../lib/protocol.js'

// Track initial opMode to detect changes requiring reboot
const initialOpMode = ref(null)
onMounted(() => { initialOpMode.value = settings.opMode })

function onModeChange(evt) {
  setSetting('opMode', Number(evt.target.value))
}

function formatUptime(ms) {
  const totalSecs = Math.floor(ms / 1000)
  const d = Math.floor(totalSecs / 86400)
  const h = Math.floor((totalSecs % 86400) / 3600)
  const m = Math.floor((totalSecs % 3600) / 60)
  const s = totalSecs % 60
  const parts = []
  if (d > 0) parts.push(`${d}d`)
  if (h > 0) parts.push(`${h}h`)
  if (m > 0) parts.push(`${m}m`)
  if (d === 0 && s > 0) parts.push(`${s}s`)
  return parts.length ? parts.join(' ') : '0s'
}
</script>

<template>
  <div class="status-bar-wrapper">
    <!-- Device Mode selector -->
    <div class="mode-selector">
      <label class="mode-label">Device Mode</label>
      <select
        class="mode-select"
        :value="settings.opMode"
        @change="onModeChange"
      >
        <option v-for="(name, idx) in OP_MODE_NAMES" :key="idx" :value="idx">
          {{ name }}
        </option>
      </select>
    </div>

    <!-- Reboot warning when mode changed -->
    <div
      v-if="initialOpMode !== null && settings.opMode !== initialOpMode"
      class="reboot-warning"
    >
      Mode change requires save &amp; reboot to take effect
    </div>

    <!-- Status grid -->
    <div class="status-bar">
      <div class="status-item">
        <span class="status-label">BLE</span>
        <span class="status-value" :class="status.connected ? 'on' : 'off'">{{ status.connected ? 'Connected' : 'Off' }}</span>
      </div>
      <div class="status-item">
        <span class="status-label">USB</span>
        <span class="status-value" :class="status.usb ? 'on' : 'off'">{{ status.usb ? 'Connected' : 'Off' }}</span>
      </div>
      <div class="status-item">
        <span class="status-label">Battery</span>
        <span class="status-value" :class="{ 'low-bat': status.bat < 15 }">{{ status.bat }}%</span>
      </div>

      <!-- Simple mode: show Profile -->
      <div v-if="settings.opMode === 0" class="status-item">
        <span class="status-label">Profile</span>
        <span class="status-value">{{ PROFILE_NAMES[status.profile] || 'NORMAL' }}</span>
      </div>

      <!-- Simulation mode: show Work Mode, Phase, Auto Profile -->
      <template v-if="settings.opMode === 1">
        <div class="status-item">
          <span class="status-label">Work Mode</span>
          <span class="status-value">{{ WORK_MODE_NAMES[status.simMode] || '---' }}</span>
        </div>
        <div class="status-item">
          <span class="status-label">Phase</span>
          <span class="status-value">{{ PHASE_NAMES[status.simPhase] || '---' }}</span>
        </div>
        <div class="status-item">
          <span class="status-label">Auto Profile</span>
          <span class="status-value">{{ PROFILE_NAMES[status.simProfile] || 'NORMAL' }}</span>
        </div>
      </template>

      <div class="status-item">
        <span class="status-label">Mode</span>
        <span class="status-value">{{ MODE_NAMES[status.mode] || 'NORMAL' }}</span>
      </div>
      <div class="status-item">
        <span class="status-label">Keyboard</span>
        <span class="status-value" :class="status.kb ? 'on' : 'off'">{{ status.kb ? 'ON' : 'OFF' }}</span>
      </div>
      <div class="status-item">
        <span class="status-label">Mouse</span>
        <span class="status-value" :class="status.ms ? 'on' : 'off'">{{ status.ms ? 'ON' : 'OFF' }}</span>
      </div>
      <div class="status-item">
        <span class="status-label">Mouse State</span>
        <span class="status-value">{{ MOUSE_STATE_NAMES[status.mouseState] || 'IDLE' }}</span>
      </div>
      <div class="status-item">
        <span class="status-label">Next Key</span>
        <span class="status-value mono">{{ status.kbNext }}</span>
      </div>
      <div class="status-item">
        <span class="status-label">Uptime</span>
        <span class="status-value mono">{{ formatUptime(status.uptime) }}</span>
      </div>
    </div>
  </div>
</template>

<style scoped>
.status-bar-wrapper {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
}
.mode-selector {
  display: flex;
  align-items: center;
  gap: 0.75rem;
  padding: 0.625rem 0.75rem;
  background: var(--surface-2);
  border-radius: 8px;
}
.mode-label {
  font-size: 0.8rem;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.05em;
  color: var(--text-dim);
  white-space: nowrap;
}
.mode-select {
  flex: 1;
  padding: 0.4rem 0.6rem;
  border-radius: 6px;
  border: 1px solid var(--border);
  background: var(--surface-1);
  color: var(--text);
  font-size: 0.9rem;
  font-weight: 600;
  cursor: pointer;
}
.reboot-warning {
  padding: 0.5rem 0.75rem;
  border-radius: 6px;
  background: rgba(255, 167, 38, 0.1);
  border: 1px solid var(--warning);
  color: var(--warning);
  font-size: 0.8rem;
  font-weight: 500;
}
.status-bar {
  display: flex;
  flex-wrap: wrap;
  gap: 0.5rem;
  padding: 0.75rem;
  background: var(--surface-2);
  border-radius: 8px;
}
.status-item {
  display: flex;
  flex-direction: column;
  align-items: center;
  flex: 1 1 auto;
  min-width: 80px;
  padding: 0.25rem 0.5rem;
}
.status-label {
  font-size: 0.7rem;
  text-transform: uppercase;
  letter-spacing: 0.05em;
  color: var(--text-dim);
}
.status-value {
  font-size: 0.95rem;
  font-weight: 600;
}
.status-value.on { color: var(--accent); }
.status-value.off { color: var(--text-dim); }
.status-value.low-bat { color: #dc3545; }
.mono { font-family: 'SF Mono', 'Fira Code', monospace; }
</style>
