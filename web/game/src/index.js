import React from 'react';
import ReactDOM from 'react-dom/client';
import { initI18n } from '../../shared/i18n';
import App from './App';
import './styles.css';

// Initialise i18next for the Game bundle.
// Locale files are fetched at runtime from /shared/locales/{lng}/{ns}.json
initI18n({ ns: ['common', 'game'], defaultNS: 'common' }).then(() => {
  const root = ReactDOM.createRoot(document.getElementById('root'));
  root.render(
    <React.StrictMode>
      <App />
    </React.StrictMode>
  );
});

