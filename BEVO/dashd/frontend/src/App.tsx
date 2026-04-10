import React from 'react';
import './App.css';
import 'bootstrap/dist/css/bootstrap.min.css';
import Dashboard from './screens/Dashboard';
import { DashProvider } from './context/DashContext';

function App() {
  return (
    <DashProvider>
      <div className="App">
        <Dashboard />
      </div>
    </DashProvider>
  );
}

export default App;
