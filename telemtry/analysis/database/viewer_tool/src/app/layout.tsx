import type { Metadata, Viewport } from "next";
import { Geist, Geist_Mono } from "next/font/google";
import "./globals.css";
import PageLayout from "@/components/PageLayout";

import Providers from "@/components/Providers";
import { ToastContainer } from 'react-toastify';



const geistSans = Geist({
  variable: "--font-geist-sans",
  subsets: ["latin"],
});

const geistMono = Geist_Mono({
  variable: "--font-geist-mono",
  subsets: ["latin"],
});

export const metadata: Metadata = {};

// Without this, Next (App Router) emits no viewport tag and phones render the
// page at a desktop width then shrink it — unreadable trackside. Allow zoom up
// to 5x so a strategist can pinch into a dense table in the sun.
export const viewport: Viewport = {
  width: "device-width",
  initialScale: 1,
  maximumScale: 5,
  themeColor: "#16140f",
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="en">
      <body
        className={`${geistSans.variable} ${geistMono.variable} antialiased bg-gray-100`}
      >
        <Providers>
          <PageLayout>
            {children}
            <ToastContainer position="bottom-right" autoClose={2000} />
          </PageLayout>
        </Providers>
      </body>
    </html>
  );
}