import { withAuth } from "next-auth/middleware"
import { NextResponse } from 'next/server';

export default withAuth(
  function middleware(req) {
    // console.log("Middleware: ", req.nextUrl.pathname, req.nextauth.token);
  },
  {
    callbacks: {
      authorized: ({ token }) => !!token,
    },
  }
);

export const config = { matcher: ["/((?!api|login|_next/static|_next/image|favicon.ico|.*\\.png$).*)"] };