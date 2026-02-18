<script setup>
import { status } from '../lib/store.js'
import { PROFILE_NAMES, MODE_NAMES, MOUSE_STATE_NAMES } from '../lib/protocol.js'

function formatUptime(ms) {
  const secs = Math.floor(ms / 1000)
  const h = Math.floor(secs / 3600)
  const m = Math.floor((secs % 3600) / 60)
  const s = secs % 60
  return `${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`
}
</script>

<template>
  <div class="status-bar">
    <div class="status-item">
      <span class="status-label">Battery</span>
      <span class="status-value" :class="{ 'low-bat': status.bat < 15 }">{{ status.bat }}%</span>
    </div>
    <div class="status-item">
      <span class="status-label">Profile</span>
      <span class="status-value">{{ PROFILE_NAMES[status.profile] || 'NORMAL' }}</span>
    </div>
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
</template>

<style scoped>
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
