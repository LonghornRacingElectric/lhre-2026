
import NextAuth, { DefaultSession, DefaultUser } from "next-auth";
import { JWT, DefaultJWT } from "next-auth/jwt";

declare module "next-auth" {
  interface Session {
    user: {
      username: string | null;
    } & DefaultSession["user"];
  }

  interface User extends DefaultUser {
    username: string | null;
  }
}

declare module "next-auth/jwt" {
  interface JWT extends DefaultJWT {
    username: string | null;
  }
}
