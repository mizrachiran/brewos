/**
 * Authentication Middleware
 *
 * Session-based authentication using our own tokens.
 * OAuth providers are used only for identity verification at login time.
 */

import { Request, Response, NextFunction } from "express";
import {
  verifyAccessToken,
  type SessionUser,
  type Session,
} from "../services/session.js";

// Extend Express Request type
declare global {
  namespace Express {
    interface Request {
      user?: SessionUser;
      session?: Session;
    }
  }
}

/**
 * Session-based authentication middleware
 * Verifies our own access tokens (not OAuth provider tokens)
 */
export function sessionAuthMiddleware(
  req: Request,
  res: Response,
  next: NextFunction
): void {
  const authHeader = req.headers.authorization;

  if (!authHeader?.startsWith("Bearer ")) {
    res.status(401).json({ error: "Missing authorization header" });
    return;
  }

  const accessToken = authHeader.slice(7);

  try {
    const result = verifyAccessToken(accessToken);

    if (!result) {
      res.status(401).json({ error: "Invalid or expired token" });
      return;
    }

    // Attach user and session to request
    req.user = result.user;
    req.session = result.session;

    next();
  } catch (error) {
    console.error("[Auth] Token verification error:", error);
    res.status(401).json({ error: "Authentication failed" });
  }
}

/**
 * Optional session auth - doesn't require auth but attaches user if present
 */
export function optionalSessionAuthMiddleware(
  req: Request,
  _res: Response,
  next: NextFunction
): void {
  const authHeader = req.headers.authorization;

  if (authHeader?.startsWith("Bearer ")) {
    const accessToken = authHeader.slice(7);

    try {
      const result = verifyAccessToken(accessToken);

      if (result) {
        req.user = result.user;
        req.session = result.session;
      }
    } catch {
      // Ignore errors - just continue without user
    }
  }

  next();
}

/**
 * Verify access token for WebSocket connections
 * Returns user info if valid, null otherwise
 */
export function verifyWebSocketToken(accessToken: string): SessionUser | null {
  try {
    const result = verifyAccessToken(accessToken);
    return result?.user || null;
  } catch {
    return null;
  }
}
