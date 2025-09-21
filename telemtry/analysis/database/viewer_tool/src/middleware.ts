import { withAuth } from "next-auth/middleware"
import { NextResponse } from 'next/server';

export default withAuth(
  function middleware(req) {
    // console.log("Middleware: ", req.nextUrl.pathname, req.nextauth.token);
    if (req.nextUrl.pathname === '/' && !req.nextauth.token) {
      return NextResponse.redirect(new URL('/login', req.url));
    }
  },
  {
    callbacks: {
      authorized: ({ token }) => !!token,
    },
  }
);

export const config = { matcher: ["/"] };