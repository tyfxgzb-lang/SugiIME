import { updateConfig } from './config-sync';

export function applyShortcutConfig(config: {
  switch_language_shift?: boolean;
  switch_language_ctrl?: boolean;
  switch_language_ctrl_alt_space?: boolean;
  toggle_character_set_ctrl_shift_f?: boolean;
  maintain_candidate_delete?: boolean;
  maintain_clear_cache?: boolean;
  maintain_restart?: boolean;
  maintain_exit?: boolean;
} | undefined): void {
  if (!config) {
    return;
  }
  const shift = document.getElementById('switchLanguageShiftCheckbox') as HTMLInputElement | null;
  const ctrl = document.getElementById('switchLanguageCtrlCheckbox') as HTMLInputElement | null;
  const ctrlAltSpace = document.getElementById('switchLanguageCtrlAltSpaceCheckbox') as HTMLInputElement | null;
  const maintainCandidateDelete = document.getElementById('maintainCandidateDeleteCheckbox') as HTMLInputElement | null;
  const maintainClearCache = document.getElementById('maintainClearCacheCheckbox') as HTMLInputElement | null;
  const maintainRestart = document.getElementById('maintainRestartCheckbox') as HTMLInputElement | null;
  const maintainExit = document.getElementById('maintainExitCheckbox') as HTMLInputElement | null;
  if (maintainCandidateDelete && typeof config.maintain_candidate_delete === 'boolean') {
    maintainCandidateDelete.checked = config.maintain_candidate_delete;
  }
  if (maintainClearCache && typeof config.maintain_clear_cache === 'boolean') {
    maintainClearCache.checked = config.maintain_clear_cache;
  }
  if (maintainRestart && typeof config.maintain_restart === 'boolean') {
    maintainRestart.checked = config.maintain_restart;
  }
  if (maintainExit && typeof config.maintain_exit === 'boolean') {
    maintainExit.checked = config.maintain_exit;
  }
  if (shift && typeof config.switch_language_shift === 'boolean') {
    shift.checked = config.switch_language_shift;
  }
  if (ctrl && typeof config.switch_language_ctrl === 'boolean') {
    ctrl.checked = config.switch_language_ctrl;
  }
  if (ctrlAltSpace && typeof config.switch_language_ctrl_alt_space === 'boolean') {
    ctrlAltSpace.checked = config.switch_language_ctrl_alt_space;
  }
}

export function setupShortcut(): void {
  const mapping: Record<string, string> = {
    shift: 'keybindings.switch_language_shift',
    ctrl: 'keybindings.switch_language_ctrl',
    'ctrl-alt-space': 'keybindings.switch_language_ctrl_alt_space'
  };
  document.querySelectorAll<HTMLInputElement>('input[name="switch-language"]').forEach((checkbox) => {
    checkbox.addEventListener('change', () => {
      const path = mapping[checkbox.value];
      if (path) {
        updateConfig(path, checkbox.checked);
      }
    });
  });
  const maintainMapping: Record<string, string> = {
    'candidate-delete': 'keybindings.maintain_candidate_delete',
    'clear-cache': 'keybindings.maintain_clear_cache',
    restart: 'keybindings.maintain_restart',
    exit: 'keybindings.maintain_exit'
  };
  document.querySelectorAll<HTMLInputElement>('input[name="maintain-shortcut"]').forEach((checkbox) => {
    checkbox.addEventListener('change', () => {
      const path = maintainMapping[checkbox.value];
      if (path) {
        updateConfig(path, checkbox.checked);
      }
    });
  });
}
