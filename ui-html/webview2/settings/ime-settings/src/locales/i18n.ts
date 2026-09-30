import { zh } from './zh';
import { ja } from './ja';
import { en } from './en';

export type Language = 'zh' | 'ja' | 'en';

const dictionaries: Record<Language, Record<string, string>> = { zh, ja, en };

const STORAGE_KEY = 'sugiime_ui_language';
const DEFAULT_LANGUAGE: Language = 'ja';

let currentLanguage: Language = DEFAULT_LANGUAGE;
const observers = new Set<() => void>();

export function getLanguage(): Language {
  return currentLanguage;
}

export function t(key: string, fallback?: string): string {
  const dict = dictionaries[currentLanguage];
  if (dict && key in dict) return dict[key];
  // fallback chain: ja -> zh -> key itself
  if (currentLanguage !== 'ja' && ja[key]) return ja[key];
  if (currentLanguage !== 'zh' && zh[key]) return zh[key];
  return fallback ?? key;
}

export function setLanguage(lang: Language): void {
  if (!dictionaries[lang]) lang = DEFAULT_LANGUAGE;
  if (lang === currentLanguage) return;
  currentLanguage = lang;
  try {
    localStorage.setItem(STORAGE_KEY, lang);
  } catch {
    /* ignore */
  }
  document.documentElement.setAttribute('lang', lang);
  applyI18n(document.documentElement);
  observers.forEach((cb) => {
    try {
      cb();
    } catch {
      /* ignore */
    }
  });
}

export function initLanguage(): void {
  let stored: Language | null = null;
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (raw === 'zh' || raw === 'ja' || raw === 'en') stored = raw;
  } catch {
    /* ignore */
  }
  currentLanguage = stored ?? DEFAULT_LANGUAGE;
  document.documentElement.setAttribute('lang', currentLanguage);
}

export function onLanguageChange(callback: () => void): () => void {
  observers.add(callback);
  return () => observers.delete(callback);
}

let mutationObserver: MutationObserver | null = null;

function ensureObserver(): void {
  if (mutationObserver) return;
  mutationObserver = new MutationObserver((mutations) => {
    for (const mutation of mutations) {
      for (const node of mutation.addedNodes) {
        if (node instanceof HTMLElement) {
          applyI18n(node);
        }
      }
    }
  });
  mutationObserver.observe(document.body, { childList: true, subtree: true });
}

export function applyI18n(root: ParentNode = document): void {
  if (!(root instanceof Element) && root !== document) return;
  const elements = root.querySelectorAll<HTMLElement>('[data-i18n]');
  elements.forEach((el) => {
    const key = el.getAttribute('data-i18n');
    if (!key) return;
    const translated = t(key);
    if (translated !== key) el.textContent = translated;
  });
  const attrElements = root.querySelectorAll<HTMLElement>('[data-i18n-attr]');
  attrElements.forEach((el) => {
    const spec = el.getAttribute('data-i18n-attr');
    if (!spec) return;
    for (const pair of spec.split(';')) {
      const [attr, key] = pair.split(':');
      if (attr && key) {
        const translated = t(key.trim());
        if (translated !== key.trim()) el.setAttribute(attr.trim(), translated);
      }
    }
  });
}

export function startI18nObserver(): void {
  ensureObserver();
}
