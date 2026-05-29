import React from 'react';
import './App.css';
import 'bootstrap/dist/css/bootstrap.min.css';
import Dashboard from './screens/Dashboard';
import { DashProvider } from './context/DashContext';
import { SettingsProvider } from './context/SettingsContext';

function App() {
  return (
    <SettingsProvider>
      <DashProvider>
        <div className="App">
          <Dashboard />
        </div>
      </DashProvider>
    </SettingsProvider>
  );
}

export default App;
