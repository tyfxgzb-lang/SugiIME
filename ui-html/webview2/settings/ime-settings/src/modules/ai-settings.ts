import { applyDropdownValue, applyToggleState, setupDropdownMenu, setupToggleButton } from './shared';
import { updateConfig } from './config-sync';

let applyingAiConfig = false;

export function applyAiConfig(ai: {
  enabled?: boolean;
  provider?: string;
  endpoint?: string;
  model?: string;
  token?: string;
  candidate_limit?: number;
  prompt?: string;
} | undefined): void {
  applyingAiConfig = true;
  try {
    if (typeof ai?.enabled === 'boolean') {
      applyToggleState('aiEnabledToggleBtn', ai.enabled);
    }
    applyDropdownValue('aiProviderBtn', 'aiProviderMenu', ai?.provider);
    const endpointInput = document.getElementById('aiEndpointInput') as HTMLInputElement | null;
    if (endpointInput) endpointInput.value = ai?.endpoint ?? '';
    const modelInput = document.getElementById('aiModelInput') as HTMLInputElement | null;
    if (modelInput) modelInput.value = ai?.model ?? '';
    const tokenInput = document.getElementById('aiTokenInput') as HTMLInputElement | null;
    if (tokenInput) tokenInput.value = ai?.token ?? '';
    applyDropdownValue('aiCandidateLimitBtn', 'aiCandidateLimitMenu', ai?.candidate_limit != null ? String(ai.candidate_limit) : undefined);
    const promptInput = document.getElementById('aiPromptInput') as HTMLTextAreaElement | null;
    if (promptInput) promptInput.value = ai?.prompt ?? '';
  } finally {
    applyingAiConfig = false;
  }
}

export function setupAiSettings(): void {
  setupToggleButton('aiEnabledToggleBtn', (active) => {
    updateConfig('ai_assistant.enabled', active);
  });
  setupDropdownMenu('aiProviderBtn', 'aiProviderMenu', '', true, 'ai_assistant.provider');
  setupDropdownMenu('aiCandidateLimitBtn', 'aiCandidateLimitMenu', '', true, 'ai_assistant.candidate_limit',
    (value) => Number(value));

  const endpointInput = document.getElementById('aiEndpointInput') as HTMLInputElement | null;
  endpointInput?.addEventListener('change', () => {
    if (applyingAiConfig) return;
    updateConfig('ai_assistant.endpoint', endpointInput.value.trim());
  });

  const modelInput = document.getElementById('aiModelInput') as HTMLInputElement | null;
  modelInput?.addEventListener('change', () => {
    if (applyingAiConfig) return;
    updateConfig('ai_assistant.model', modelInput.value.trim());
  });

  const tokenInput = document.getElementById('aiTokenInput') as HTMLInputElement | null;
  tokenInput?.addEventListener('change', () => {
    if (applyingAiConfig) return;
    updateConfig('ai_assistant.token', tokenInput.value);
  });

  const promptInput = document.getElementById('aiPromptInput') as HTMLTextAreaElement | null;
  promptInput?.addEventListener('change', () => {
    if (applyingAiConfig) return;
    updateConfig('ai_assistant.prompt', promptInput.value);
  });
}
