<script setup>
import { ref, computed } from 'vue'
import {
  simModes, simBlocks, simDataLoading, settings,
  setWorkModeParam, setWorkModeTiming, resetSimDataToDefaults, fetchSimData,
} from '../lib/store.js'
import {
  WORK_MODE_NAMES, PROFILE_LABEL_NAMES, JOB_SIM_NAMES, formatMs,
} from '../lib/protocol.js'

const expandedMode = ref(null)
const activeProfile = ref(1) // Default to Normal

function toggleMode(idx) {
  expandedMode.value = expandedMode.value === idx ? null : idx
  activeProfile.value = 1
}

// Current job's blocks for the day schedule display
const currentBlocks = computed(() => simBlocks[settings.jobSim] || [])

// KB% bar color based on value
function kbBarColor(pct) {
  if (pct >= 70) return 'var(--accent)'
  if (pct >= 40) return 'var(--warning)'
  return '#66bb6a'
}

// --- Parameter update helpers ---

function updateKb(idx, val) {
  setWorkModeParam(idx, 'kb', val)
}

function updateWeight(idx, profile, val) {
  const fields = ['wL', 'wN', 'wB']
  const mode = simModes[idx]
  if (!mode) return
  const others = fields.reduce((sum, f, i) => i === profile ? sum : sum + mode[f], 0)
  setWorkModeParam(idx, fields[profile], Math.min(val, 100 - others))
}

function updateDurMin(idx, val) {
  setWorkModeParam(idx, 'dMin', val)
}

function updateDurMax(idx, val) {
  setWorkModeParam(idx, 'dMax', val)
}

function updateStintMin(idx, val) {
  setWorkModeParam(idx, 'pMin', val)
}

function updateStintMax(idx, val) {
  setWorkModeParam(idx, 'pMax', val)
}

function updateTimingField(modeIdx, profileIdx, field, val) {
  const mode = simModes[modeIdx]
  if (!mode) return
  const t = { ...mode.timing[profileIdx] }
  t[field] = Number(val)
  setWorkModeTiming(modeIdx, profileIdx, t)
}

// Timing field definitions for the per-profile editor
const timingFieldPairs = [
  {
    help: 'Number of keys pressed in a rapid burst before pausing.',
    fields: [
      { key: 'burstKeysMin', label: 'Burst Keys Min', max: 255, step: 1 },
      { key: 'burstKeysMax', label: 'Burst Keys Max', max: 255, step: 1 },
    ],
  },
  {
    help: 'Delay between individual keystrokes within a burst.',
    fields: [
      { key: 'interKeyMinMs', label: 'Inter-Key Min', max: 2000, step: 10, fmt: true },
      { key: 'interKeyMaxMs', label: 'Inter-Key Max', max: 2000, step: 10, fmt: true },
    ],
  },
  {
    help: 'Pause between typing bursts (thinking time).',
    fields: [
      { key: 'burstGapMinMs', label: 'Burst Gap Min', max: 120000, step: 500, fmt: true },
      { key: 'burstGapMaxMs', label: 'Burst Gap Max', max: 120000, step: 500, fmt: true },
    ],
  },
  {
    help: 'How long each key is held down before release.',
    fields: [
      { key: 'keyHoldMinMs', label: 'Key Hold Min', max: 1000, step: 10, fmt: true },
      { key: 'keyHoldMaxMs', label: 'Key Hold Max', max: 1000, step: 10, fmt: true },
    ],
  },
  {
    help: 'Duration of mouse movement phases.',
    fields: [
      { key: 'mouseDurMinMs', label: 'Mouse Dur Min', max: 120000, step: 500, fmt: true },
      { key: 'mouseDurMaxMs', label: 'Mouse Dur Max', max: 120000, step: 500, fmt: true },
    ],
  },
  {
    help: 'Idle pauses where no input is sent (reading/thinking).',
    fields: [
      { key: 'idleDurMinMs', label: 'Idle Dur Min', max: 120000, step: 500, fmt: true },
      { key: 'idleDurMaxMs', label: 'Idle Dur Max', max: 120000, step: 500, fmt: true },
    ],
  },
]

function modeName(modeId) {
  return WORK_MODE_NAMES[modeId] || `Mode ${modeId}`
}
</script>

<template>
  <section class="card">
    <h2>Sim Tuning</h2>

    <div v-if="simDataLoading" class="loading">Loading work modes...</div>

    <div v-else-if="simModes.length === 0" class="loading">
      <button class="btn" @click="fetchSimData">Load Sim Data</button>
    </div>

    <template v-else>
      <!-- Mode list -->
      <div class="mode-list">
        <div
          v-for="(mode, idx) in simModes"
          :key="idx"
          class="mode-item"
          :class="{ expanded: expandedMode === idx }"
        >
          <!-- Header row -->
          <div class="mode-header" @click="toggleMode(idx)">
            <span class="mode-name">{{ mode?.name || WORK_MODE_NAMES[idx] || '???' }}</span>
            <div class="kb-bar-wrap">
              <div
                class="kb-bar"
                :style="{ width: (mode?.kb || 0) + '%', background: kbBarColor(mode?.kb || 0) }"
              />
              <span class="kb-label">{{ mode?.kb || 0 }}% KB</span>
            </div>
            <span class="chevron">{{ expandedMode === idx ? '\u25B2' : '\u25BC' }}</span>
          </div>

          <!-- Expanded editor -->
          <div v-if="expandedMode === idx && mode" class="mode-editor">
            <!-- KB Ratio -->
            <div class="field">
              <label>
                KB Ratio
                <span class="field-value">{{ mode.kb }}%</span>
              </label>
              <input
                type="range"
                min="0" max="100"
                :value="mode.kb"
                @input="updateKb(idx, Number($event.target.value))"
              />
              <p class="help-text">Keyboard vs mouse activity split. Higher = more typing.</p>
            </div>

            <!-- Profile Weights -->
            <div class="subsection">
              <h3>Profile Weights</h3>
              <div class="weight-grid" :class="{ 'weights-under': mode.wL + mode.wN + mode.wB < 100 }">
                <div v-for="(pName, pIdx) in PROFILE_LABEL_NAMES" :key="pIdx" class="field">
                  <label>
                    {{ pName }}
                    <span class="field-value">{{ [mode.wL, mode.wN, mode.wB][pIdx] }}%</span>
                  </label>
                  <input
                    type="range"
                    min="0" max="100"
                    :value="[mode.wL, mode.wN, mode.wB][pIdx]"
                    @input="updateWeight(idx, pIdx, Number($event.target.value))"
                  />
                </div>
              </div>
              <p class="help-text">
                Probability weights for auto-profile selection.
                Sum: {{ mode.wL + mode.wN + mode.wB }}%
              </p>
            </div>

            <!-- Mode Duration -->
            <div class="subsection">
              <h3>Mode Duration</h3>
              <div class="range-pair">
                <div class="field">
                  <label>Min <span class="field-value">{{ mode.dMin }}s</span></label>
                  <input type="range" min="10" max="3600" step="10" :value="mode.dMin"
                    @input="updateDurMin(idx, Number($event.target.value))" />
                </div>
                <div class="field">
                  <label>Max <span class="field-value">{{ mode.dMax }}s</span></label>
                  <input type="range" min="10" max="3600" step="10" :value="mode.dMax"
                    @input="updateDurMax(idx, Number($event.target.value))" />
                </div>
              </div>
              <p class="help-text">How long this work mode stays active before the orchestrator picks another mode from the current block.</p>
            </div>

            <!-- Profile Stint Duration -->
            <div class="subsection">
              <h3>Profile Stint</h3>
              <div class="range-pair">
                <div class="field">
                  <label>Min <span class="field-value">{{ mode.pMin }}s</span></label>
                  <input type="range" min="5" max="600" step="5" :value="mode.pMin"
                    @input="updateStintMin(idx, Number($event.target.value))" />
                </div>
                <div class="field">
                  <label>Max <span class="field-value">{{ mode.pMax }}s</span></label>
                  <input type="range" min="5" max="600" step="5" :value="mode.pMax"
                    @input="updateStintMax(idx, Number($event.target.value))" />
                </div>
              </div>
              <p class="help-text">How long a profile (Lazy/Normal/Busy) runs before switching to another, based on profile weights.</p>
            </div>

            <!-- Per-Profile Timing -->
            <div class="subsection">
              <h3>Phase Timing</h3>
              <div class="profile-tabs">
                <button
                  v-for="(pName, pIdx) in PROFILE_LABEL_NAMES"
                  :key="pIdx"
                  class="tab-btn"
                  :class="{ active: activeProfile === pIdx }"
                  @click="activeProfile = pIdx"
                >
                  {{ pName }}
                </button>
              </div>
              <div v-if="mode.timing[activeProfile]">
                <div v-for="(pair, pairIdx) in timingFieldPairs" :key="pairIdx" class="timing-pair">
                  <div class="timing-grid">
                    <div v-for="tf in pair.fields" :key="tf.key" class="field field-compact">
                      <label>
                        {{ tf.label }}
                        <span class="field-value">
                          {{ tf.fmt ? formatMs(mode.timing[activeProfile][tf.key]) : mode.timing[activeProfile][tf.key] }}
                        </span>
                      </label>
                      <input
                        type="range"
                        :min="0"
                        :max="tf.max"
                        :step="tf.step"
                        :value="mode.timing[activeProfile][tf.key]"
                        @input="updateTimingField(idx, activeProfile, tf.key, $event.target.value)"
                      />
                    </div>
                  </div>
                  <p class="help-text">{{ pair.help }}</p>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>

      <!-- Day Schedule (read-only) -->
      <div class="subsection" v-if="currentBlocks.length > 0">
        <h3>Day Schedule ({{ JOB_SIM_NAMES[settings.jobSim] }})</h3>
        <div class="schedule-table">
          <div class="sched-row sched-header">
            <span class="sched-name">Block</span>
            <span class="sched-time">Start</span>
            <span class="sched-dur">Dur</span>
            <span class="sched-modes">Modes</span>
          </div>
          <div v-for="(block, bIdx) in currentBlocks" :key="bIdx" class="sched-row">
            <span class="sched-name">{{ block.name }}</span>
            <span class="sched-time">{{ Math.floor(block.startMin / 60) }}:{{ String(block.startMin % 60).padStart(2, '0') }}</span>
            <span class="sched-dur">{{ block.durMin }}m</span>
            <span class="sched-modes">
              <span v-for="(m, mIdx) in block.modes" :key="mIdx" class="mode-tag">
                {{ modeName(m.modeId) }} {{ m.weight }}%
              </span>
            </span>
          </div>
        </div>
        <p class="help-text">Read-only schedule from the {{ JOB_SIM_NAMES[settings.jobSim] }} template. Times are relative offsets from job start.</p>
      </div>

      <!-- Actions -->
      <div class="actions">
        <button
          class="btn btn-danger"
          @click="resetSimDataToDefaults"
        >
          Reset to Defaults
        </button>
      </div>
    </template>
  </section>
</template>

<style scoped>
.loading {
  text-align: center;
  padding: 1rem;
  color: var(--text-dim);
}

.mode-list {
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.mode-item {
  border: 1px solid var(--border);
  border-radius: 6px;
  overflow: hidden;
}

.mode-item.expanded {
  border-color: var(--accent);
}

.mode-header {
  display: flex;
  align-items: center;
  gap: 0.75rem;
  padding: 0.6rem 0.75rem;
  cursor: pointer;
  background: var(--surface-2);
  transition: background 0.15s;
}

.mode-header:hover {
  background: var(--surface-1);
}

.mode-name {
  font-weight: 600;
  font-size: 0.85rem;
  min-width: 80px;
}

.kb-bar-wrap {
  flex: 1;
  position: relative;
  height: 18px;
  background: rgba(255,255,255,0.05);
  border-radius: 3px;
  overflow: hidden;
}

.kb-bar {
  height: 100%;
  border-radius: 3px;
  transition: width 0.3s ease;
  opacity: 0.7;
}

.kb-label {
  position: absolute;
  right: 6px;
  top: 50%;
  transform: translateY(-50%);
  font-size: 0.7rem;
  font-weight: 600;
  font-family: 'SF Mono', 'Fira Code', monospace;
  color: var(--text);
}

.chevron {
  font-size: 0.7rem;
  color: var(--text-dim);
}

.mode-editor {
  padding: 0.75rem;
  background: var(--surface-1);
  border-top: 1px solid var(--border);
}

.subsection {
  margin-top: 1rem;
  padding-top: 0.75rem;
  border-top: 1px solid var(--border);
}

.subsection h3 {
  font-size: 0.8rem;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.04em;
  color: var(--text-dim);
  margin-bottom: 0.5rem;
}

.weight-grid {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 0.5rem;
}

.weight-grid.weights-under input[type="range"] {
  filter: brightness(0.5);
}

.weight-grid.weights-under .field-value {
  color: #1a5276;
}

.range-pair {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 0.75rem;
}

.profile-tabs {
  display: flex;
  gap: 4px;
  margin-bottom: 0.75rem;
}

.tab-btn {
  flex: 1;
  padding: 0.35rem 0.5rem;
  border: 1px solid var(--border);
  border-radius: 4px;
  background: var(--surface-2);
  color: var(--text-dim);
  font-size: 0.8rem;
  cursor: pointer;
  transition: all 0.15s;
}

.tab-btn.active {
  background: var(--accent);
  color: #000;
  border-color: var(--accent);
  font-weight: 600;
}

.timing-pair {
  margin-bottom: 0.5rem;
}

.timing-pair .help-text {
  margin-top: 0.15rem;
}

.timing-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 0.5rem 0.75rem;
}

.field-compact label {
  font-size: 0.75rem;
}

.field-compact .field-value {
  font-size: 0.75rem;
}

/* Schedule table */
.schedule-table {
  font-size: 0.8rem;
  border: 1px solid var(--border);
  border-radius: 4px;
  overflow: hidden;
}

.sched-row {
  display: grid;
  grid-template-columns: 100px 50px 45px 1fr;
  gap: 0.25rem;
  padding: 0.35rem 0.5rem;
  align-items: center;
}

.sched-row:nth-child(even) {
  background: var(--surface-2);
}

.sched-header {
  background: var(--surface-2) !important;
  font-weight: 600;
  color: var(--text-dim);
  text-transform: uppercase;
  font-size: 0.7rem;
  letter-spacing: 0.04em;
}

.sched-time, .sched-dur {
  font-family: 'SF Mono', 'Fira Code', monospace;
  color: var(--text-dim);
}

.sched-modes {
  display: flex;
  flex-wrap: wrap;
  gap: 3px;
}

.mode-tag {
  font-size: 0.7rem;
  padding: 1px 5px;
  border-radius: 3px;
  background: rgba(79, 195, 247, 0.1);
  color: var(--accent);
  white-space: nowrap;
}

/* Actions */
.actions {
  display: flex;
  gap: 0.75rem;
  margin-top: 1rem;
  padding-top: 0.75rem;
  border-top: 1px solid var(--border);
}
</style>
