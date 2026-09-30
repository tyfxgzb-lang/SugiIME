let candidateSurfaceThemeListener: ((theme: ResolvedTheme) => void) | null = null;

export function setCandidateSurfaceThemeListener(listener: ((theme: ResolvedTheme) => void) | null): void {
  candidateSurfaceThemeListener = listener;
}

export type ThemeMode = 'dark' | 'light' | 'system';
export type SurfaceTheme = 'follow' | 'dark' | 'light';
export type ResolvedTheme = 'dark' | 'light';

export type ThemeConfig = {
  theme_mode?: string;
  theme_settings?: string;
  theme_cand?: string;
  theme_ftb?: string;
  theme_menu?: string;
  theme_emoji?: string;
  theme_screen_keyboard?: string;
  theme_handwriting?: string;
  theme_voice?: string;
};

import { t } from '../locales/i18n';

function themeModeLabel(mode: ThemeMode): string {
  if (mode === 'light') return t('appearance.themeLight');
  if (mode === 'system') return t('appearance.themeSystem');
  return t('appearance.themeDark');
}

function surfaceThemeLabel(surface: SurfaceTheme): string {
  if (surface === 'dark') return t('appearance.themeDark');
  if (surface === 'light') return t('appearance.themeLight');
  return t('appearance.themeFollowGlobal');
}

let mediaQuery: MediaQueryList | null = null;
let lastThemeConfig: ThemeConfig | null = null;

function normalizeThemeMode(value: string | undefined): ThemeMode {
  if (value === 'light' || value === 'dark' || value === 'system') {
    return value;
  }
  if (value === 'auto') {
    return 'system';
  }
  return 'dark';
}

function normalizeSurfaceTheme(value: string | undefined): SurfaceTheme {
  if (value === 'light' || value === 'dark' || value === 'follow') {
    return value;
  }
  return 'follow';
}

export function isSystemLightTheme(): boolean {
  return window.matchMedia?.('(prefers-color-scheme: light)').matches ?? false;
}

export function resolveTheme(mode: ThemeMode, surface: SurfaceTheme = 'follow'): ResolvedTheme {
  if (surface === 'dark' || surface === 'light') {
    return surface;
  }
  if (mode === 'system') {
    return isSystemLightTheme() ? 'light' : 'dark';
  }
  return mode;
}

export function applyDocumentTheme(theme: ResolvedTheme): void {
  const root = document.documentElement;
  root.setAttribute('data-theme', theme);
  root.style.colorScheme = theme;

  const meta = document.querySelector('meta[name="color-scheme"]');
  if (meta) {
    meta.setAttribute('content', theme);
  }

  document.body?.setAttribute('data-theme', theme);
  document.body?.classList.toggle('theme-light', theme === 'light');
  document.body?.classList.toggle('theme-dark', theme === 'dark');
}

export function applyCandidatePreviewTheme(theme: ResolvedTheme): void {
  document.querySelectorAll('.cand-preview .candidate').forEach((element) => {
    element.classList.toggle('theme-light', theme === 'light');
    element.classList.toggle('theme-dark', theme === 'dark');
  });
  candidateSurfaceThemeListener?.(theme);
}

function ensureSystemThemeListener(onChange: () => void): void {
  if (!window.matchMedia) {
    return;
  }
  if (!mediaQuery) {
    mediaQuery = window.matchMedia('(prefers-color-scheme: light)');
  }
  mediaQuery.onchange = () => onChange();
}

export function applyThemeConfig(config: ThemeConfig | undefined): void {
  lastThemeConfig = config ?? null;
  const mode = normalizeThemeMode(config?.theme_mode);
  const settingsTheme = normalizeSurfaceTheme(config?.theme_settings);
  const candTheme = normalizeSurfaceTheme(config?.theme_cand);

  const settingsResolved = resolveTheme(mode, settingsTheme);
  const candResolved = resolveTheme(mode, candTheme);

  applyDocumentTheme(settingsResolved);
  applyCandidatePreviewTheme(candResolved);
  applyFtbPreviewTheme(resolveTheme(mode, normalizeSurfaceTheme(config?.theme_ftb)));
  if (document.querySelector('.screenkb-preview')) {
    const screenKeyboardTheme = resolveTheme(mode, normalizeSurfaceTheme(config?.theme_screen_keyboard));
    void import('./screenkb-settings').then((module) => {
      module.applyScreenKeyboardPreviewTheme(screenKeyboardTheme);
    });
  }

  applyDropdownLabel('themeBtn', themeModeLabel(mode), mode);
  applyDropdownLabel('settingsThemeBtn', surfaceThemeLabel(settingsTheme), settingsTheme);
  applyDropdownLabel('candThemeBtn', surfaceThemeLabel(candTheme), candTheme);
  applyDropdownLabel('ftbThemeBtn', surfaceThemeLabel(normalizeSurfaceTheme(config?.theme_ftb)), normalizeSurfaceTheme(config?.theme_ftb));
  applyDropdownLabel('menuThemeBtn', surfaceThemeLabel(normalizeSurfaceTheme(config?.theme_menu)), normalizeSurfaceTheme(config?.theme_menu));
  applyDropdownLabel('emojiThemeBtn', surfaceThemeLabel(normalizeSurfaceTheme(config?.theme_emoji)), normalizeSurfaceTheme(config?.theme_emoji));
  applyDropdownLabel('screenKeyboardThemeBtn', surfaceThemeLabel(normalizeSurfaceTheme(config?.theme_screen_keyboard)), normalizeSurfaceTheme(config?.theme_screen_keyboard));
  applyDropdownLabel('handwritingThemeBtn', surfaceThemeLabel(normalizeSurfaceTheme(config?.theme_handwriting)), normalizeSurfaceTheme(config?.theme_handwriting));
  applyDropdownLabel('voiceThemeBtn', surfaceThemeLabel(normalizeSurfaceTheme(config?.theme_voice)), normalizeSurfaceTheme(config?.theme_voice));

  ensureSystemThemeListener(() => {
    const modeNow = normalizeThemeMode(config?.theme_mode);
    const settingsNow = normalizeSurfaceTheme(config?.theme_settings);
    const candNow = normalizeSurfaceTheme(config?.theme_cand);
    const ftbNow = normalizeSurfaceTheme(config?.theme_ftb);
    const menuNow = normalizeSurfaceTheme(config?.theme_menu);
    const emojiNow = normalizeSurfaceTheme(config?.theme_emoji);
    const screenKeyboardNow = normalizeSurfaceTheme(config?.theme_screen_keyboard);
    const handwritingNow = normalizeSurfaceTheme(config?.theme_handwriting);
    const voiceNow = normalizeSurfaceTheme(config?.theme_voice);
    if (modeNow !== 'system' &&
        settingsNow !== 'follow' &&
        candNow !== 'follow' &&
        ftbNow !== 'follow' &&
        menuNow !== 'follow' &&
        emojiNow !== 'follow' &&
        screenKeyboardNow !== 'follow' &&
        handwritingNow !== 'follow' &&
        voiceNow !== 'follow') {
      return;
    }
    applyThemeConfig(config);
  });

  updateSkinThemeCard(mode, settingsResolved, candResolved);
}

// Skin, screen keyboard and handwriting pages load after the first snapshot has been applied, so their previews only exist once the theme has already been resolved; they call this when their DOM is ready.
export function reapplyThemeConfig(): void {
  if (lastThemeConfig) {
    applyThemeConfig(lastThemeConfig);
  }
}

export function applyFtbPreviewTheme(theme: ResolvedTheme): void {
  document.querySelectorAll('.ftb-preview-host:not(#skinToolbarPreview):not([data-skin-toolbar])').forEach((element) => {
    element.classList.toggle('theme-light', theme === 'light');
    element.classList.toggle('theme-dark', theme === 'dark');
  });
}

function applyDropdownLabel(btnId: string, label: string, value?: string): void {
  const btn = document.getElementById(btnId);
  const span = btn?.querySelector<HTMLElement>('span');
  if (span) {
    span.textContent = label;
  }
  if (value !== undefined && btn) {
    btn.dataset.selected = value;
  }
}

function updateSkinThemeCard(
  mode: ThemeMode,
  settingsResolved: ResolvedTheme,
  candResolved: ResolvedTheme
): void {
  const description = document.getElementById('skinThemeStatus');
  if (description) {
    description.textContent =
      `${t('appearance.themeMode')} ${themeModeLabel(mode)} · ${t('appearance.settingsTheme')} ${settingsResolved === 'light' ? t('appearance.themeLight') : t('appearance.themeDark')} · ${t('appearance.candTheme')} ${candResolved === 'light' ? t('appearance.themeLight') : t('appearance.themeDark')}`;
  }

  // Fluent preview follows the same resolved theme as the real candidate surface.
  if (document.querySelector('[data-skin-switch]')) {
    void import('./skin').then((module) => {
      module.syncSkinPreviewTheme(candResolved);
    });
  }
}

export function setThemeMode(value: string | undefined): void {
  const mode = normalizeThemeMode(value);
  const current: ThemeConfig = {
    theme_mode: mode,
    theme_settings: readSurfaceFromUi('settingsThemeMenu', 'settingsThemeBtn'),
    theme_cand: readSurfaceFromUi('candThemeMenu', 'candThemeBtn'),
    theme_ftb: readSurfaceFromUi('ftbThemeMenu', 'ftbThemeBtn'),
    theme_menu: readSurfaceFromUi('menuThemeMenu', 'menuThemeBtn'),
    theme_emoji: readSurfaceFromUi('emojiThemeMenu', 'emojiThemeBtn'),
    theme_screen_keyboard: readSurfaceFromUi('screenKeyboardThemeMenu', 'screenKeyboardThemeBtn'),
    theme_handwriting: readSurfaceFromUi('handwritingThemeMenu', 'handwritingThemeBtn'),
    theme_voice: readSurfaceFromUi('voiceThemeMenu', 'voiceThemeBtn')
  };
  applyThemeConfig(current);
}

export function setSurfaceTheme(
  surface: 'settings' | 'cand' | 'ftb' | 'menu' | 'emoji' | 'screenKeyboard' | 'handwriting' | 'voice',
  value: string | undefined
): void {
  const current: ThemeConfig = {
    theme_mode: readModeFromUi(),
    theme_settings: readSurfaceFromUi('settingsThemeMenu', 'settingsThemeBtn'),
    theme_cand: readSurfaceFromUi('candThemeMenu', 'candThemeBtn'),
    theme_ftb: readSurfaceFromUi('ftbThemeMenu', 'ftbThemeBtn'),
    theme_menu: readSurfaceFromUi('menuThemeMenu', 'menuThemeBtn'),
    theme_emoji: readSurfaceFromUi('emojiThemeMenu', 'emojiThemeBtn'),
    theme_screen_keyboard: readSurfaceFromUi('screenKeyboardThemeMenu', 'screenKeyboardThemeBtn'),
    theme_handwriting: readSurfaceFromUi('handwritingThemeMenu', 'handwritingThemeBtn'),
    theme_voice: readSurfaceFromUi('voiceThemeMenu', 'voiceThemeBtn')
  };

  if (surface === 'settings') current.theme_settings = normalizeSurfaceTheme(value);
  if (surface === 'cand') current.theme_cand = normalizeSurfaceTheme(value);
  if (surface === 'ftb') current.theme_ftb = normalizeSurfaceTheme(value);
  if (surface === 'menu') current.theme_menu = normalizeSurfaceTheme(value);
  if (surface === 'emoji') current.theme_emoji = normalizeSurfaceTheme(value);
  if (surface === 'screenKeyboard') current.theme_screen_keyboard = normalizeSurfaceTheme(value);
  if (surface === 'handwriting') current.theme_handwriting = normalizeSurfaceTheme(value);
  if (surface === 'voice') current.theme_voice = normalizeSurfaceTheme(value);

  applyThemeConfig(current);
}

function readModeFromUi(): ThemeMode {
  const btn = document.getElementById('themeBtn');
  const selected = btn?.dataset.selected;
  if (selected === 'light' || selected === 'dark' || selected === 'system') return selected;
  return 'dark';
}

function readSurfaceFromUi(menuId: string, btnId: string): SurfaceTheme {
  const btn = document.getElementById(btnId);
  const selected = btn?.dataset.selected;
  if (selected === 'light' || selected === 'dark' || selected === 'follow') return selected;
  const selItem = document.querySelector<HTMLElement>(`#${menuId} .dropdown-item.active`);
  return normalizeSurfaceTheme(selItem?.dataset.value);
}
