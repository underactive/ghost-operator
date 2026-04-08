<script setup>
import { ref } from 'vue'
import { settings, setSetting, resetDefaults, rebootDevice, decoyNames, exportSettings, importSettings, platform } from '../lib/store.js'

const showResetConfirm = ref(false)
const showRebootConfirm = ref(false)
const importing = ref(false)
const importResult = ref(null)
const importError = ref('')
const fileInput = ref(null)

function onDecoyChange(e) {
  setSetting('decoy', Number(e.target.value))
}

function onNameInput(e) {
  // Limit to 14 chars, printable ASCII only
  let val = e.target.value.replace(/[^\x20-\x7E]/g, '').substring(0, 14)
  setSetting('name', val)
}

function confirmReset() {
  showResetConfirm.value = false
  resetDefaults()
}

function confirmReboot() {
  showRebootConfirm.value = false
  rebootDevice()
}

function triggerImport() {
  fileInput.value?.click()
}

async function onFileSelected(e) {
  const file = e.target.files?.[0]
  if (!file) return
  // Reset file input so the same file can be re-selected
  e.target.value = ''

  importing.value = true
  importResult.value = null
  importError.value = ''

  try {
    const result = await importSettings(file)
    importResult.value = result
  } catch (err) {
    importError.value = err.message
  } finally {
    importing.value = false
    // Clear feedback after 5 seconds
    setTimeout(() => {
      importResult.value = null
      importError.value = ''
    }, 5000)
  }
}
</script>

<template>
  <section class="card">
    <h2>Device</h2>
    <div class="field">
      <label>BLE Identity <span class="help-text">(requires reboot)</span></label>
      <select :value="settings.decoy" @change="onDecoyChange">
        <option v-for="(name, idx) in decoyNames" :key="idx + 1" :value="idx + 1">
          {{ name }}
        </option>
        <option :value="0">Custom</option>
      </select>
    </div>

    <div class="field" :class="{ 'field-disabled': settings.decoy !== 0 }">
      <label>Custom Name</label>
      <input
        type="text"
        :value="settings.name"
        maxlength="14"
        :disabled="settings.decoy !== 0"
        @input="onNameInput"
      />
    </div>

    <template v-if="platform !== 'c6' && platform !== 's3'">
      <div class="field">
        <label>BT while USB <span class="help-text">(keep Bluetooth on when USB plugged in)</span></label>
        <select :value="settings.btWhileUsb" @change="setSetting('btWhileUsb', Number($event.target.value))">
          <option :value="0">Off</option>
          <option :value="1">On</option>
        </select>
      </div>

      <div class="field">
        <label>Dashboard Link <span class="help-text">(show link on USB connect; reboot to apply)</span></label>
        <select :value="settings.dashboard" @change="setSetting('dashboard', Number($event.target.value))">
          <option :value="0">Off</option>
          <option :value="1">On</option>
        </select>
      </div>
    </template>

    <div class="backup-section">
      <label>Settings Backup</label>
      <div class="backup-buttons">
        <button class="btn" @click="exportSettings">Export</button>
        <button class="btn" @click="triggerImport" :disabled="importing">
          {{ importing ? 'Importing...' : 'Import' }}
        </button>
        <input
          ref="fileInput"
          type="file"
          accept=".json"
          style="display: none"
          @change="onFileSelected"
        />
      </div>
      <div v-if="importResult" class="import-feedback import-success">
        Restored {{ importResult.applied }} settings.
        <span v-if="importResult.skipped.length"> Skipped {{ importResult.skipped.length }}.</span>
        <span v-if="importResult.errors.length"> {{ importResult.errors.length }} errors.</span>
      </div>
      <div v-if="importError" class="import-feedback import-error">
        {{ importError }}
      </div>
    </div>

    <div class="actions">
      <div v-if="!showResetConfirm">
        <button class="btn btn-danger" @click="showResetConfirm = true">
          Reset Defaults
        </button>
      </div>
      <div v-else class="confirm-row">
        <span>Reset all settings?</span>
        <button class="btn btn-danger" @click="confirmReset">Yes, Reset</button>
        <button class="btn" @click="showResetConfirm = false">Cancel</button>
      </div>

      <div v-if="!showRebootConfirm">
        <button class="btn btn-warning" @click="showRebootConfirm = true">
          Reboot Device
        </button>
      </div>
      <div v-else class="confirm-row">
        <span>Reboot device?</span>
        <button class="btn btn-warning" @click="confirmReboot">Yes, Reboot</button>
        <button class="btn" @click="showRebootConfirm = false">Cancel</button>
      </div>
    </div>
  </section>
</template>

<style scoped>
.actions {
  display: flex;
  gap: 1rem;
  margin-top: 1rem;
  flex-wrap: wrap;
}
.confirm-row {
  display: flex;
  align-items: center;
  gap: 0.5rem;
}
.confirm-row span {
  font-size: 0.9rem;
}
.field-disabled {
  opacity: 0.4;
  pointer-events: none;
}
.backup-section {
  margin-top: 1rem;
  padding-top: 1rem;
  border-top: 1px solid var(--border-color, #333);
}
.backup-section > label {
  display: block;
  font-weight: 500;
  margin-bottom: 0.5rem;
}
.backup-buttons {
  display: flex;
  gap: 0.5rem;
}
.import-feedback {
  margin-top: 0.5rem;
  font-size: 0.85rem;
  padding: 0.4rem 0.6rem;
  border-radius: 4px;
}
.import-success {
  color: #4ade80;
}
.import-error {
  color: #f87171;
}
</style>
