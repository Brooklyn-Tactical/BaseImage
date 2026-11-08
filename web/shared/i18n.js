import i18next from 'i18next';
import HttpBackend from 'i18next-http-backend';
import LanguageDetector from 'i18next-browser-languagedetector';
import { initReactI18next } from 'react-i18next';

/**
 * Supported languages.
 * To add a new language:
 *   1. Create  web/shared/locales/<code>/{common,game,system}.json
 *   2. Add an entry to this array: { code: '<code>', label: '<Display Name>' }
 */
export const LANGUAGES = [
    { code: 'cs', label: 'Čeština' },
    { code: 'de', label: 'Deutsch' },
    { code: 'en', label: 'English' },
    { code: 'es', label: 'Español' },
    { code: 'fr', label: 'Français' },
    { code: 'ja', label: '日本語' },
    { code: 'ko', label: '한국어' },
    { code: 'pl', label: 'Polski' },
    { code: 'pt', label: 'Português' },
    { code: 'zh', label: '中文' },
];

/**
 * Configure and initialize the shared i18next singleton.
 *
 * Call this ONCE from each app's index.js, BEFORE ReactDOM.render/createRoot,
 * passing the namespaces that particular bundle needs:
 *
 *   Game UI:   initI18n({ ns: ['common', 'game'],   defaultNS: 'common' })
 *   System UI: initI18n({ ns: ['common', 'system'], defaultNS: 'common' })
 *
 * Locale files are loaded at runtime from /shared/locales/{lng}/{ns}.json
 * so they are never bundled and are shared by both apps on the filesystem.
 *
 * Namespace / key naming convention:
 *   common  — battery messages, generic action labels (Save, Cancel …),
 *             version labels, language picker; used by both bundles.
 *   game    — Game UI strings: app bar, info drawer, settings card,
 *             game control buttons, snackbar messages.
 *   system  — System UI strings: nav drawer, all page content.
 *
 * Key format: <section>.<subsection?>.<name>
 *   Examples: battery.level  |  action.save  |  settings.countdown.label
 *
 * @param {string[]} ns       Namespaces for this bundle, e.g. ['common', 'game']
 * @param {string}   defaultNS Default namespace (default: 'common')
 * @returns {Promise} Resolves when i18next has loaded the initial locale files.
 */
export function initI18n({ ns, defaultNS = 'common' }) {
  return i18next
    .use(HttpBackend)
    .use(LanguageDetector)
    .use(initReactI18next)
    .init({
      ns,
      defaultNS,
      fallbackLng: 'en',
      supportedLngs: LANGUAGES.map(l => l.code),
      backend: {
        loadPath: '/shared/locales/{{lng}}/{{ns}}.json',
      },
      detection: {
        // Check localStorage first (persists user selection),
        // then fall back to the browser's Accept-Language header.
        order: ['localStorage', 'navigator'],
        caches: ['localStorage'],
        lookupLocalStorage: 'i18nextLng',
      },
      interpolation: {
        escapeValue: false, // React already escapes output
      },
      react: {
        useSuspense: false, // Prevents blank flash while locale files load
      },
    });
}

export default i18next;
