<script setup>
import { connectionState, dirty, saving, saveToFlash, statusMessage, dfuActive } from './lib/store.js'
import ConnectButton from './components/ConnectButton.vue'
import StatusBar from './components/StatusBar.vue'
import BatteryChart from './components/BatteryChart.vue'
import TimingSection from './components/TimingSection.vue'
import KeySlotEditor from './components/KeySlotEditor.vue'
import MouseSection from './components/MouseSection.vue'
import ProfileSection from './components/ProfileSection.vue'
import ScheduleSection from './components/ScheduleSection.vue'
import DisplaySection from './components/DisplaySection.vue'
import DeviceSection from './components/DeviceSection.vue'
import FirmwareUpdate from './components/FirmwareUpdate.vue'
</script>

<template>
  <div class="app">
    <header>
      <h1>👻 Ghost Operator</h1>
      <span class="version">v1.10.2</span>
    </header>

    <ConnectButton v-if="!dfuActive || connectionState.connected" />

    <template v-if="connectionState.connected || dfuActive">
      <StatusBar v-if="connectionState.connected" />
      <BatteryChart v-if="connectionState.connected" />

      <div class="settings-grid">
        <template v-if="connectionState.connected">
          <TimingSection />
          <MouseSection />
          <KeySlotEditor />
          <ProfileSection />
          <ScheduleSection />
          <DisplaySection />
          <DeviceSection />
        </template>
        <FirmwareUpdate />
      </div>

      <footer v-if="connectionState.connected" class="save-bar">
        <span class="save-status" :class="{ dirty: dirty }">
          {{ statusMessage || (dirty ? 'Unsaved changes' : 'All saved') }}
        </span>
        <button
          class="btn btn-save"
          :disabled="!dirty || saving"
          @click="saveToFlash"
        >
          {{ saving ? 'Saving...' : 'Save to Flash' }}
        </button>
      </footer>
    </template>

    <div v-if="!connectionState.connected && !dfuActive" class="placeholder">
      <p>Connect to a Ghost Operator device via USB to manage settings.</p>
    </div>
  </div>
</template>

<style>
/* ====== CSS Custom Properties (Dark theme) ====== */
:root {
  --bg: #0f0f0f;
  --surface-1: #1a1a1a;
  --surface-2: #242424;
  --border: #333;
  --text: #e0e0e0;
  --text-dim: #888;
  --accent: #4fc3f7;
  --accent-hover: #29b6f6;
  --danger: #ef5350;
  --warning: #ffa726;
}

* {
  box-sizing: border-box;
  margin: 0;
  padding: 0;
}

body {
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
  background: var(--bg);
  color: var(--text);
  line-height: 1.5;
}

/* ====== Layout ====== */
.app {
  max-width: 720px;
  margin: 0 auto;
  padding: 1.5rem 1rem 6rem;
}

header {
  display: flex;
  align-items: baseline;
  gap: 0.75rem;
  margin-bottom: 1rem;
}

header h1 {
  font-size: 1.5rem;
  font-weight: 700;
}

.version {
  font-size: 0.8rem;
  color: var(--text-dim);
  font-family: 'SF Mono', 'Fira Code', monospace;
}

.placeholder {
  text-align: center;
  padding: 3rem 1rem;
  color: var(--text-dim);
}

/* ====== Settings Grid ====== */
.settings-grid {
  display: flex;
  flex-direction: column;
  gap: 1rem;
  margin-top: 1rem;
}

/* ====== Cards ====== */
.card {
  background: var(--surface-1);
  border: 1px solid var(--border);
  border-radius: 8px;
  padding: 1rem 1.25rem;
}

.card h2 {
  font-size: 1rem;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.05em;
  color: var(--accent);
  margin-bottom: 0.75rem;
}

/* ====== Form Fields ====== */
.field {
  margin-bottom: 0.75rem;
}

.field label {
  display: flex;
  justify-content: space-between;
  align-items: baseline;
  font-size: 0.85rem;
  margin-bottom: 0.25rem;
}

.field-value {
  font-weight: 600;
  font-family: 'SF Mono', 'Fira Code', monospace;
  color: var(--accent);
}

.field input[type="range"] {
  width: 100%;
  accent-color: var(--accent);
}

.field input[type="text"],
.field select {
  width: 100%;
  padding: 0.5rem 0.75rem;
  border-radius: 6px;
  border: 1px solid var(--border);
  background: var(--surface-2);
  color: var(--text);
  font-size: 0.9rem;
}

.field select {
  cursor: pointer;
}

.help-text {
  font-size: 0.8rem;
  color: var(--text-dim);
}

/* ====== Buttons ====== */
.btn {
  padding: 0.5rem 1.25rem;
  border: 1px solid var(--border);
  border-radius: 6px;
  background: var(--surface-2);
  color: var(--text);
  font-size: 0.9rem;
  cursor: pointer;
  transition: background 0.15s, border-color 0.15s;
}

.btn:hover:not(:disabled) {
  background: var(--surface-1);
  border-color: var(--text-dim);
}

.btn:disabled {
  opacity: 0.4;
  cursor: not-allowed;
}

.btn-connect {
  background: var(--accent);
  color: #000;
  border-color: var(--accent);
  font-weight: 600;
  padding: 0.75rem 2rem;
  font-size: 1rem;
}

.btn-connect:hover:not(:disabled) {
  background: var(--accent-hover);
  border-color: var(--accent-hover);
}

.btn-disconnect {
  border-color: var(--text-dim);
}

.btn-save {
  background: var(--accent);
  color: #000;
  border-color: var(--accent);
  font-weight: 600;
}

.btn-save:hover:not(:disabled) {
  background: var(--accent-hover);
}

.btn-danger {
  color: var(--danger);
  border-color: var(--danger);
}

.btn-danger:hover:not(:disabled) {
  background: rgba(239, 83, 80, 0.1);
}

.btn-warning {
  color: var(--warning);
  border-color: var(--warning);
}

.btn-warning:hover:not(:disabled) {
  background: rgba(255, 167, 38, 0.1);
}

/* ====== Save Bar (sticky footer) ====== */
.save-bar {
  position: fixed;
  bottom: 0;
  left: 0;
  right: 0;
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 0.75rem 1.5rem;
  background: var(--surface-1);
  border-top: 1px solid var(--border);
  z-index: 100;
}

.save-status {
  font-size: 0.85rem;
  color: var(--text-dim);
}

.save-status.dirty {
  color: var(--warning);
}
</style>
