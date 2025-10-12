import { NextRequest, NextResponse } from 'next/server';
import fs from 'fs/promises';
import path from 'path';

export async function GET(req: NextRequest) {
  try {
    const configPath = path.join(process.cwd(), '..', '..', '..', 'net_configs.json');
    const configFile = await fs.readFile(configPath, 'utf-8');
    const config = JSON.parse(configFile);

    if (config.CLIENT_TARGET === 'LOCAL') {
      return NextResponse.redirect('http://localhost:3000');
    } else {
      return NextResponse.redirect('https://lhrelectric.org/grafana');
    }
  } catch (error) {
    console.error('Error reading net_configs.json:', error);
    // Default redirect if config is not found
    return NextResponse.redirect('https://lhrelectric.org/grafana');
  }
}
