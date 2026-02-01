import React from 'react';
import { render, screen } from '@testing-library/react';
import App from './App';

test('renders dashboard with speed gauge', () => {
  render(<App />);
  // Check for one of the gauge labels
  const linkElement = screen.getByText(/Speed/i);
  expect(linkElement).toBeInTheDocument();
});
