<script setup>
import { ref } from 'vue'
import { settings, setSetting, resetDefaults, rebootDevice } from '../lib/store.js'

const showResetConfirm = ref(false)
const showRebootConfirm = ref(false)

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
</script>

<template>
  <section class="card">
    <h2>Device</h2>
    <div class="field">
      <label>Device Name <span class="help-text">(requires reboot)</span></label>
      <input
        type="text"
        :value="settings.name"
        maxlength="14"
        @input="onNameInput"
      />
    </div>

    <div class="field">
      <label>BT while USB <span class="help-text">(keep Bluetooth on when USB plugged in)</span></label>
      <select :value="settings.btWhileUsb" @change="setSetting('btWhileUsb', Number($event.target.value))">
        <option :value="0">Off</option>
        <option :value="1">On</option>
      </select>
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
</style>
