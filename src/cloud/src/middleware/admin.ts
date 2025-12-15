/**
 * Admin Authentication Middleware
 *
 * Protects admin routes by verifying the user has admin privileges.
 * Admin status is stored in the profiles table (is_admin column).
 */

import { Request, Response, NextFunction } from "express";
import { sessionAuthMiddleware } from "./auth.js";
import { getDb } from "../lib/database.js";

/**
 * Check if a user has admin privileges
 */
export function checkUserIsAdmin(userId: string): boolean {
  const db = getDb();
  const result = db.exec(`SELECT is_admin FROM profiles WHERE id = ?`, [
    userId,
  ]);

  if (result.length === 0 || result[0].values.length === 0) {
    return false;
  }

  return result[0].values[0][0] === 1;
}

/**
 * Get count of admin users in the system
 */
export function getAdminCount(): number {
  const db = getDb();
  const result = db.exec(
    `SELECT COUNT(*) as count FROM profiles WHERE is_admin = 1`
  );

  if (result.length === 0 || result[0].values.length === 0) {
    return 0;
  }

  return result[0].values[0][0] as number;
}

/**
 * Admin authentication middleware
 *
 * This middleware:
 * 1. First verifies the user has a valid session (via sessionAuthMiddleware)
 * 2. Then checks if the user has admin privileges
 *
 * Use this middleware on all /api/admin/* routes
 */
export function adminAuthMiddleware(
  req: Request,
  res: Response,
  next: NextFunction
): void {
  // First, verify the session
  sessionAuthMiddleware(req, res, () => {
    // Session is valid, now check admin status
    const userId = req.user?.id;

    if (!userId) {
      res.status(401).json({ error: "Not authenticated" });
      return;
    }

    if (!checkUserIsAdmin(userId)) {
      res.status(403).json({ error: "Admin access required" });
      return;
    }

    // User is authenticated and is an admin
    next();
  });
}

/**
 * Optional admin check - doesn't block, just adds isAdmin to request
 * Useful for routes that need to know admin status but don't require it
 */
export function optionalAdminCheckMiddleware(
  req: Request,
  _res: Response,
  next: NextFunction
): void {
  const userId = req.user?.id;

  if (userId) {
    // Add isAdmin to the request for later use
    (req as Request & { isAdmin?: boolean }).isAdmin = checkUserIsAdmin(userId);
  }

  next();
}

