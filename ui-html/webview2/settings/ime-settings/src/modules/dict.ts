import { createDictionaryPager, DICTIONARY_PAGE_SIZE } from '../utils/dictionary-pagination';
import { readDictionaryFile } from '../utils/dictionary-file';
import { confirmDialog } from '../utils/confirm-dialog';
import { hoistOverlay } from '../utils/overlay-host';
import { onHostMessage } from '../utils/host-messages';
import type { SettingsMessage } from '../../../../shared/messages';
type DictionaryRequest = Extract<SettingsMessage, { type: 'dictionaryRequest' }>['data'];
import { serializeHostMessage } from '../../../../shared/messages';
import { t } from '../locales/i18n';
type DictionaryType = 'quanpin' | 'wubi' | 'english' | 'japanese';
type DictionaryRow = { code?: string; word: string; weight?: number; display?: string };

let dictionary: DictionaryType = 'japanese';
let editing: DictionaryRow | null = null;
let requestCounter = 0;
let lastQuery = '';
const pendingRequests = new Map<string, { action: DictionaryRequest['action']; dictionary: string }>();
let latestQueryId = '';
let pager: ReturnType<typeof createDictionaryPager> | undefined;
let toastTimer: number | null = null;

function post(action: DictionaryRequest['action'], data: Partial<Omit<DictionaryRequest, 'action' | 'requestId'>> = {}): void {
  const requestId = `dict-${++requestCounter}`;
  const targetDictionary = dictionary;
  pendingRequests.set(requestId, { action, dictionary: targetDictionary });
  if (action === 'query') { latestQueryId = requestId; pager?.loading(); }
  window.chrome?.webview?.postMessage(serializeHostMessage({
    type: 'dictionaryRequest',
    data: { requestId, dictionary, action, ...data }
  }));
}

function showToast(message: string, ok: boolean, durationMs = 3200): void {
  const toast = document.getElementById('dictToast');
  if (!toast) return;
  const messageElement = document.getElementById('dictToastMessage');
  const iconElement = document.getElementById('dictToastIcon');
  if (messageElement) messageElement.textContent = message;
  if (iconElement) iconElement.textContent = ok ? '' : '!';
  toast.className = `dict-toast visible ${ok ? 'success' : 'error'}`;
  if (toastTimer !== null) window.clearTimeout(toastTimer);
  toastTimer = window.setTimeout(() => {
    toast.classList.remove('visible');
    toastTimer = null;
  }, durationMs);
}

function renderRows(rows: DictionaryRow[]): void {
  const body = document.getElementById('dictRows');
  if (!body) return;
  if (!rows.length) {
    body.innerHTML = `<tr><td colspan="5" class="dict-empty">${t('dict.noEntries')}</td></tr>`;
    syncTableHeaderWidth();
    return;
  }
  body.replaceChildren(...rows.map((row, index) => {
    const tr = document.createElement('tr');
    const indexCell = document.createElement('td');
    indexCell.className = 'dict-index-column';
    indexCell.textContent = String((pager?.offset ?? 0) + index + 1);
    tr.appendChild(indexCell);
    const values = dictionary === 'english'
      ? [row.word, row.display ?? row.word, String(row.weight ?? 0)]
      : [row.code ?? '', row.word, String(row.weight ?? 0)];
    values.forEach((value) => {
      const td = document.createElement('td');
      td.textContent = value;
      td.addEventListener('mouseenter', () => {
        if (td.scrollWidth > td.clientWidth) td.title = value;
        else td.removeAttribute('title');
      });
      tr.appendChild(td);
    });
    const actions = document.createElement('td');
    const edit = document.createElement('button'); edit.className = 'dict-row-action'; edit.textContent = t('common.edit');
    edit.addEventListener('click', () => openDialog(row));
    const remove = document.createElement('button'); remove.className = 'dict-row-action danger'; remove.textContent = t('common.delete');
    remove.addEventListener('click', async () => {
      if (!await confirmDialog(`${t('dict.deleteConfirm')}“${row.word}”？`)) return;
      if (dictionary === 'english') post('delete', { oldWord: row.word, oldDisplay: row.display ?? row.word, word: row.word, display: row.display ?? row.word });
      else post('delete', { oldCode: row.code, oldWord: row.word, code: row.code, word: row.word, weight: row.weight });
    });
    actions.append(edit, remove); tr.appendChild(actions); return tr;
  }));
  syncTableHeaderWidth();
}

function syncTableHeaderWidth(): void {
  window.requestAnimationFrame(() => {
    const scrollArea = document.querySelector<HTMLElement>('.dict-table-wrap');
    const header = document.getElementById('dictTableHeaderWrap');
    if (!scrollArea || !header) return;
    header.style.paddingRight = `${scrollArea.offsetWidth - scrollArea.clientWidth}px`;
  });
}

function updateMode(): void {
  latestQueryId = '';
  pager?.reset();
  const english = dictionary === 'english';
  const quanpin = dictionary === 'quanpin';
  const japanese = dictionary === 'japanese';
  const search = document.getElementById('dictSearch') as HTMLInputElement;
  search.value = '';
  // Japanese user dictionary is import-only: imported words are merged straight
  // into the live Japanese candidates, so query/add/table UI is hidden there.
  const setShown = (id: string, shown: boolean) => {
    const el = document.getElementById(id);
    if (el) el.style.display = shown ? '' : 'none';
  };
  setShown('dictSearch', !japanese);
  setShown('dictSearchButton', !japanese);
  setShown('dictAddButton', !japanese);
  setShown('dictTableHeaderWrap', !japanese);
  const tableWrap = document.querySelector<HTMLElement>('.dict-table-wrap');
  if (tableWrap) tableWrap.style.display = japanese ? 'none' : '';
  search.placeholder = english ? t('dict.searchPlaceholderEnglish') : quanpin
    ? t('dict.searchPlaceholderQuanpin') : t('dict.searchPlaceholderWubi');
  document.getElementById('dictHint')!.textContent = japanese
    ? t('dict.hintJapanese')
    : english
      ? t('dict.hintEnglish')
      : quanpin
        ? t('dict.hintQuanpin')
        : t('dict.hintWubi');
  const importButton = document.getElementById('dictImportButton') as HTMLButtonElement | null;
  if (importButton) importButton.style.display = '';
  document.getElementById('dictTableHeader')!.innerHTML = english
    ? `<th class="dict-index-column">No.</th><th>${t('dict.colWord')}</th><th>${t('dict.colDisplay')}</th><th>${t('common.weight')}</th><th>${t('common.operation')}</th>`
    : `<th class="dict-index-column">No.</th><th>${t('dict.colCode')}</th><th>${t('common.word')}</th><th>${t('common.weight')}</th><th>${t('common.operation')}</th>`;
  document.getElementById('dictRows')!.innerHTML = japanese
    ? `<tr><td colspan="5" class="dict-empty">${t('dict.emptyJapanese')}</td></tr>`
    : `<tr><td colspan="5" class="dict-empty">${t('dict.emptyOther')}</td></tr>`;
  syncTableHeaderWidth();
}

function openDialog(row: DictionaryRow | null = null): void {
  editing = row;
  const english = dictionary === 'english';
  document.getElementById('dictDialogTitle')!.textContent = row ? t('dict.modalEditTitle') : t('dict.modalAddTitle');
  document.getElementById('dictCodeField')!.firstChild!.textContent = english ? t('dict.colWord') : dictionary === 'quanpin' ? t('dict.colQuanpin') : t('dict.colWubi');
  document.getElementById('dictWordField')!.firstChild!.textContent = english ? t('dict.colDisplay') : t('common.word');
  (document.getElementById('dictCode') as HTMLInputElement).value = row
    ? (english ? row.word : row.code ?? '')
    : '';
  document.getElementById('dictWeightField')!.style.display = 'grid';
  (document.getElementById('dictWord') as HTMLInputElement).value = english ? row?.display ?? '' : row?.word ?? '';
  (document.getElementById('dictWeight') as HTMLInputElement).value = row?.weight === undefined ? '10' : String(row.weight);
  const modal = document.getElementById('dictModal')!; modal.classList.add('open'); modal.setAttribute('aria-hidden', 'false');
}

function closeDialog(): void {
  const modal = document.getElementById('dictModal')!; modal.classList.remove('open'); modal.setAttribute('aria-hidden', 'true'); editing = null;
}

function query(offset = 0): void {
  if (dictionary === 'japanese') return;
  if (offset === 0) lastQuery = (document.getElementById('dictSearch') as HTMLInputElement).value.trim();
  if (!lastQuery) { showToast('请输入查询内容', false); return; }
  post('query', { ...(dictionary === 'english' ? { word: lastQuery } : { code: lastQuery }), offset, limit: DICTIONARY_PAGE_SIZE });
}

function downloadExport(content: string, filename: string): void {
  const blob = new Blob([new Uint8Array([0xef, 0xbb, 0xbf]), content], {
    type: 'text/plain;charset=utf-8',
  });
  const url = URL.createObjectURL(blob);
  const anchor = document.createElement('a');
  anchor.href = url;
  anchor.download = filename;
  anchor.style.display = 'none';
  document.body.appendChild(anchor);
  anchor.click();
  anchor.remove();
  window.setTimeout(() => URL.revokeObjectURL(url), 1000);
}

export function setupDictionary(): void {
  hoistOverlay(document.getElementById('dictToast'));
  hoistOverlay(document.getElementById('dictModal'));
  const table = document.querySelector<HTMLElement>('.dict-table-wrap');
  if (table) pager = createDictionaryPager(table, offset => query(offset));
  document.querySelectorAll<HTMLButtonElement>('.dict-tab').forEach((tab) => tab.addEventListener('click', () => {
    document.querySelector('.dict-tab.active')?.classList.remove('active'); tab.classList.add('active');
    dictionary = tab.dataset.dictionary as DictionaryType; updateMode();
  }));
  document.getElementById('dictSearchButton')?.addEventListener('click', () => query());
  document.getElementById('dictSearch')?.addEventListener('keydown', (event) => { if ((event as KeyboardEvent).key === 'Enter') query(); });
  document.getElementById('dictAddButton')?.addEventListener('click', () => openDialog());
  document.getElementById('dictImportButton')?.addEventListener('click', () => {
    (document.getElementById('dictImportFile') as HTMLInputElement | null)?.click();
  });
  document.getElementById('dictImportFile')?.addEventListener('change', async (event) => {
    const input = event.target as HTMLInputElement;
    const file = input.files?.[0];
    input.value = '';
    if (!file) return;
    try {
      const content = await readDictionaryFile(file);
      if (!content.trim()) { showToast('文件内容为空', false); return; }
      post('import', { content });
    } catch {
      showToast('读取文件失败', false);
    }
  });
  document.getElementById('dictImportHansButton')?.addEventListener('click', () => {
    (document.getElementById('dictImportHansFile') as HTMLInputElement | null)?.click();
  });
  document.getElementById('dictImportHansFile')?.addEventListener('change', async (event) => {
    const input = event.target as HTMLInputElement;
    const file = input.files?.[0];
    input.value = '';
    if (!file) return;
    try {
      const content = await readDictionaryFile(file);
      if (!content.trim()) { showToast('文件内容为空', false); return; }
      post('importHans', { content });
    } catch {
      showToast('读取文件失败', false);
    }
  });
  document.querySelectorAll<HTMLButtonElement>('[data-export-dictionary]').forEach((button) => {
    button.addEventListener('click', () => {
      const exportDictionary = button.dataset.exportDictionary as DictionaryType | undefined;
      if (!exportDictionary) return;
      post('export', { dictionary: exportDictionary });
    });
  });
  document.getElementById('dictCancelButton')?.addEventListener('click', closeDialog);
  document.getElementById('dictToastClose')?.addEventListener('click', () => {
    document.getElementById('dictToast')?.classList.remove('visible');
    if (toastTimer !== null) {
      window.clearTimeout(toastTimer);
      toastTimer = null;
    }
  });
  window.addEventListener('resize', syncTableHeaderWidth);
  document.addEventListener('keydown', (event: KeyboardEvent) => {
    if (event.key === 'Escape' && document.getElementById('dictModal')?.classList.contains('open')) {
      event.preventDefault();
      closeDialog();
      (document.activeElement as HTMLElement | null)?.blur();
    }
  });
  document.getElementById('dictSaveButton')?.addEventListener('click', () => {
    const code = (document.getElementById('dictCode') as HTMLInputElement).value.trim();
    const word = (document.getElementById('dictWord') as HTMLInputElement).value.trim();
    const weight = Number((document.getElementById('dictWeight') as HTMLInputElement).value);
    if (dictionary === 'english') post(editing ? 'update' : 'create', {
      word: code, display: word, weight, oldWord: editing?.word, oldDisplay: editing?.display ?? editing?.word,
    });
    else post(editing ? 'update' : 'create', { code, word, weight, oldCode: editing?.code, oldWord: editing?.word });
  });
  onHostMessage('dictionaryResponse', payload => {
    if (!payload.requestId.startsWith('dict-')) return;
    const context = pendingRequests.get(payload.requestId);
    if (!context) return;
    pendingRequests.delete(payload.requestId);
    if (context.action === 'query' && payload.requestId !== latestQueryId) return;
    if (context.action === 'query' && context.dictionary !== dictionary) return;
    const lastAction = context.action;
    if (lastAction === 'query') {
      if (payload.ok) pager?.update(payload.offset ?? 0, payload.rows.length, payload.hasMore === true);
      else pager?.failed();
    }

    const isImport = lastAction === 'import' || lastAction === 'importHans';
    const isExport = lastAction === 'export';
    if (isExport && payload.ok && typeof payload.content === 'string' && typeof payload.filename === 'string') {
      downloadExport(payload.content, payload.filename);
    }
    showToast(
      payload.message ?? (payload.ok ? '操作成功' : '操作失败'),
      Boolean(payload.ok),
      isImport ? 5600 : 3200,
    );
    if (lastAction === 'query' && payload.ok) renderRows(payload.rows);
    if (payload.ok && lastAction !== 'query' && !isExport && context.dictionary === dictionary) { closeDialog(); if (lastQuery) query(pager?.offset ?? 0); }
  });
  updateMode();
}
