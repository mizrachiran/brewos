/**
 * Authentication Routes
 *
 * Handles OAuth provider authentication and session management.
 * OAuth providers (Google, Apple, etc.) are used ONLY for identity verification.
 * After verification, we issue our own session tokens.
 */

import { Router, Request, Response } from "express";
import rateLimit from "express-rate-limit";
import { OAuth2Client } from "google-auth-library";
import {
  createSession,
  refreshSession,
  revokeSession,
  revokeAllUserSessions,
  getUserSessions,
} from "../services/session.js";
import { ensureProfile } from "../services/device.js";
import { sessionAuthMiddleware } from "../middleware/auth.js";

const router = Router();

// ============================================================================
// Rate Limiters
// ============================================================================

// General rate limiter applied to ALL routes in this router
// 60 requests per 15 minutes per IP
const generalLimiter = rateLimit({
  windowMs: 15 * 60 * 1000,
  max: 60,
  standardHeaders: true,
  legacyHeaders: false,
  message: { error: "Too many requests, please try again later" },
});

// Strict limiter for login/refresh - prevents brute force
// 5 requests per minute per IP
const authStrictLimiter = rateLimit({
  windowMs: 60 * 1000,
  max: 5,
  standardHeaders: true,
  legacyHeaders: false,
  message: { error: "Too many login attempts, please try again later" },
});

// Apply general rate limiting to ALL routes in this router
router.use(generalLimiter);

// ============================================================================
// Routes
// ============================================================================

// Google OAuth client
const GOOGLE_CLIENT_ID = process.env.GOOGLE_CLIENT_ID || "";
const googleClient = new OAuth2Client(GOOGLE_CLIENT_ID);

/**
 * POST /api/auth/google
 *
 * Exchange Google ID token for our session tokens.
 * This is the ONLY time we verify with Google.
 * Extra strict rate limiting to prevent abuse
 */
router.post(
  "/google",
  authStrictLimiter,
  async (req: Request, res: Response) => {
    try {
      const { credential } = req.body;

      if (!credential) {
        return res.status(400).json({ error: "Missing credential" });
      }

      if (!GOOGLE_CLIENT_ID) {
        console.error("[Auth] GOOGLE_CLIENT_ID not configured");
        return res.status(500).json({ error: "Auth not configured" });
      }

      let payload;
      try {
        const ticket = await googleClient.verifyIdToken({
          idToken: credential,
          audience: GOOGLE_CLIENT_ID,
        });
        payload = ticket.getPayload();
      } catch (error) {
        console.error("[Auth] Google token verification failed:", error);
        return res.status(401).json({ error: "Invalid Google credential" });
      }

      if (!payload?.sub || !payload?.email) {
        return res.status(401).json({ error: "Invalid token payload" });
      }

      ensureProfile(payload.sub, payload.email, payload.name, payload.picture);

      const metadata = {
        userAgent: req.headers["user-agent"],
        ipAddress: req.ip || req.socket.remoteAddress,
      };

      const tokens = createSession(payload.sub, metadata);

      console.log(`[Auth] User ${payload.email} logged in via Google`);

      res.json({
        accessToken: tokens.accessToken,
        refreshToken: tokens.refreshToken,
        expiresAt: tokens.accessExpiresAt,
        user: {
          id: payload.sub,
          email: payload.email,
          name: payload.name || null,
          picture: payload.picture || null,
        },
      });
    } catch (error) {
      console.error("[Auth] Login error:", error);
      res.status(500).json({ error: "Authentication failed" });
    }
  }
);

/**
 * POST /api/auth/refresh
 *
 * Refresh session using refresh token.
 * Implements refresh token rotation for security.
 */
router.post("/refresh", authStrictLimiter, (req: Request, res: Response) => {
  try {
    const { refreshToken } = req.body;

    if (!refreshToken) {
      console.warn("[Auth] Refresh called without token");
      return res.status(400).json({ error: "Missing refresh token" });
    }

    console.log(
      "[Auth] Refresh attempt with token prefix:",
      refreshToken?.substring(0, 8) + "..."
    );

    const tokens = refreshSession(refreshToken);

    if (!tokens) {
      console.warn("[Auth] Refresh failed - token not found or expired");
      return res
        .status(401)
        .json({ error: "Invalid or expired refresh token" });
    }

    console.log("[Auth] Refresh successful, new access token issued");
    res.json({
      accessToken: tokens.accessToken,
      refreshToken: tokens.refreshToken,
      expiresAt: tokens.accessExpiresAt,
    });
  } catch (error) {
    console.error("[Auth] Refresh error:", error);
    res.status(500).json({ error: "Failed to refresh session" });
  }
});

/**
 * POST /api/auth/logout
 *
 * Revoke current session.
 */
router.post("/logout", sessionAuthMiddleware, (req: Request, res: Response) => {
  try {
    const sessionId = req.session?.id;

    if (sessionId) {
      revokeSession(sessionId);
    }

    res.json({ success: true });
  } catch (error) {
    console.error("[Auth] Logout error:", error);
    res.status(500).json({ error: "Failed to logout" });
  }
});

/**
 * POST /api/auth/logout-all
 *
 * Revoke all sessions for the current user.
 */
router.post(
  "/logout-all",
  sessionAuthMiddleware,
  (req: Request, res: Response) => {
    try {
      const userId = req.user?.id;

      if (userId) {
        const count = revokeAllUserSessions(userId);
        res.json({ success: true, sessionsRevoked: count });
      } else {
        res.status(401).json({ error: "Not authenticated" });
      }
    } catch (error) {
      console.error("[Auth] Logout-all error:", error);
      res.status(500).json({ error: "Failed to logout" });
    }
  }
);

/**
 * GET /api/auth/sessions
 *
 * Get all active sessions for current user.
 */
router.get(
  "/sessions",
  sessionAuthMiddleware,
  (req: Request, res: Response) => {
    try {
      const userId = req.user?.id;

      if (!userId) {
        return res.status(401).json({ error: "Not authenticated" });
      }

      const sessions = getUserSessions(userId);
      const currentSessionId = req.session?.id;

      res.json({
        sessions: sessions.map((s) => ({
          id: s.id,
          userAgent: s.user_agent,
          ipAddress: s.ip_address,
          createdAt: s.created_at,
          lastUsedAt: s.last_used_at,
          isCurrent: s.id === currentSessionId,
        })),
      });
    } catch (error) {
      console.error("[Auth] Get sessions error:", error);
      res.status(500).json({ error: "Failed to get sessions" });
    }
  }
);

/**
 * DELETE /api/auth/sessions/:id
 *
 * Revoke a specific session.
 */
router.delete(
  "/sessions/:id",
  sessionAuthMiddleware,
  (req: Request, res: Response) => {
    try {
      const { id } = req.params;
      const userId = req.user?.id;

      if (!userId) {
        return res.status(401).json({ error: "Not authenticated" });
      }

      const sessions = getUserSessions(userId);
      const sessionToRevoke = sessions.find((s) => s.id === id);

      if (!sessionToRevoke) {
        return res.status(404).json({ error: "Session not found" });
      }

      revokeSession(id);
      res.json({ success: true });
    } catch (error) {
      console.error("[Auth] Revoke session error:", error);
      res.status(500).json({ error: "Failed to revoke session" });
    }
  }
);

/**
 * GET /api/auth/me
 *
 * Get current user info.
 */
router.get("/me", sessionAuthMiddleware, (req: Request, res: Response) => {
  try {
    const user = req.user;

    if (!user) {
      return res.status(401).json({ error: "Not authenticated" });
    }

    res.json({
      user: {
        id: user.id,
        email: user.email,
        name: user.displayName,
        picture: user.avatarUrl,
      },
    });
  } catch (error) {
    console.error("[Auth] Get user error:", error);
    res.status(500).json({ error: "Failed to get user" });
  }
});

export default router;
