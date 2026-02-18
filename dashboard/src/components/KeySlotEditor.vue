<script setup>
import { settings, availableKeys, setSetting } from '../lib/store.js'

function onSlotChange(slotIndex, event) {
  const newSlots = [...settings.slots]
  newSlots[slotIndex] = Number(event.target.value)
  setSetting('slots', newSlots.join(','))
}
</script>

<template>
  <section class="card">
    <h2>Key Slots</h2>
    <p class="help-text">Configure which keys are sent. Slot 0 is the primary key.</p>
    <div class="slots-grid">
      <div v-for="(slotVal, idx) in settings.slots" :key="idx" class="slot">
        <label class="slot-label">Slot {{ idx }}</label>
        <select :value="slotVal" @change="onSlotChange(idx, $event)">
          <option
            v-for="(keyName, keyIdx) in availableKeys"
            :key="keyIdx"
            :value="keyIdx"
          >
            {{ keyName }}
          </option>
        </select>
      </div>
    </div>
  </section>
</template>

<style scoped>
.slots-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(120px, 1fr));
  gap: 0.5rem;
}
.slot {
  display: flex;
  flex-direction: column;
  gap: 0.25rem;
}
.slot-label {
  font-size: 0.75rem;
  text-transform: uppercase;
  letter-spacing: 0.05em;
  color: var(--text-dim);
}
.slot select {
  padding: 0.4rem;
  border-radius: 4px;
  border: 1px solid var(--border);
  background: var(--surface-1);
  color: var(--text);
  font-size: 0.9rem;
}
</style>
