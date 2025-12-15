/**
 * Admin Routes
 *
 * Protected admin API endpoints for system management.
 * All routes require admin authentication via adminAuthMiddleware.
 */

import { Router, Request, Response } from "express";
import rateLimit from "express-rate-limit";
import { adminAuthMiddleware } from "../middleware/admin.js";
import {
  getSystemStats,
  getAllUsers,
  getUserById,
  deleteUser,
  promoteToAdmin,
  demoteFromAdmin,
  createImpersonationToken,
  getAllDevices,
  getDeviceById,
  forceDeleteDevice,
  getAllSessions,
  revokeAnySession,
} from "../services/admin.js";

const router = Router();

// =============================================================================
// Rate Limiters
// =============================================================================

// General rate limiter for admin routes
const adminLimiter = rateLimit({
  windowMs: 15 * 60 * 1000, // 15 minutes
  max: 200, // Higher limit for admin operations
  standardHeaders: true,
  legacyHeaders: false,
  message: { error: "Too many requests, please try again later" },
});

// Stricter limiter for sensitive operations
const sensitiveLimiter = rateLimit({
  windowMs: 15 * 60 * 1000,
  max: 30,
  standardHeaders: true,
  legacyHeaders: false,
  message: { error: "Too many requests, please try again later" },
});

// Apply general rate limiting and admin auth to all routes
router.use(adminLimiter);
router.use(adminAuthMiddleware);

// =============================================================================
// System Stats
// =============================================================================

/**
 * GET /api/admin/stats
 * Get system-wide statistics
 */
router.get("/stats", (_req: Request, res: Response) => {
  try {
    const stats = getSystemStats();
    res.json(stats);
  } catch (error) {
    console.error("[Admin] Failed to get stats:", error);
    res.status(500).json({ error: "Failed to get system stats" });
  }
});

// =============================================================================
// User Management
// =============================================================================

/**
 * GET /api/admin/users
 * List all users with pagination and search
 */
router.get("/users", (req: Request, res: Response) => {
  try {
    const page = parseInt(req.query.page as string) || 1;
    const pageSize = Math.min(parseInt(req.query.pageSize as string) || 20, 100);
    const search = req.query.search as string | undefined;

    const result = getAllUsers(page, pageSize, search);
    res.json(result);
  } catch (error) {
    console.error("[Admin] Failed to get users:", error);
    res.status(500).json({ error: "Failed to get users" });
  }
});

/**
 * GET /api/admin/users/:id
 * Get detailed user information
 */
router.get("/users/:id", (req: Request, res: Response) => {
  try {
    const { id } = req.params;
    const user = getUserById(id);

    if (!user) {
      return res.status(404).json({ error: "User not found" });
    }

    res.json(user);
  } catch (error) {
    console.error("[Admin] Failed to get user:", error);
    res.status(500).json({ error: "Failed to get user" });
  }
});

/**
 * DELETE /api/admin/users/:id
 * Delete a user
 */
router.delete("/users/:id", sensitiveLimiter, (req: Request, res: Response) => {
  try {
    const { id } = req.params;
    const result = deleteUser(id);

    if (!result.success) {
      return res.status(400).json({ error: result.error });
    }

    res.json({ success: true });
  } catch (error) {
    console.error("[Admin] Failed to delete user:", error);
    res.status(500).json({ error: "Failed to delete user" });
  }
});

/**
 * POST /api/admin/users/:id/promote
 * Promote a user to admin
 */
router.post("/users/:id/promote", sensitiveLimiter, (req: Request, res: Response) => {
  try {
    const { id } = req.params;
    const result = promoteToAdmin(id);

    if (!result.success) {
      return res.status(400).json({ error: result.error });
    }

    console.log(`[Admin] User ${id} promoted to admin by ${req.user?.id}`);
    res.json({ success: true });
  } catch (error) {
    console.error("[Admin] Failed to promote user:", error);
    res.status(500).json({ error: "Failed to promote user" });
  }
});

/**
 * POST /api/admin/users/:id/demote
 * Demote a user from admin
 */
router.post("/users/:id/demote", sensitiveLimiter, (req: Request, res: Response) => {
  try {
    const { id } = req.params;
    const requestingAdminId = req.user?.id;

    if (!requestingAdminId) {
      return res.status(401).json({ error: "Not authenticated" });
    }

    const result = demoteFromAdmin(id, requestingAdminId);

    if (!result.success) {
      return res.status(400).json({ error: result.error });
    }

    console.log(`[Admin] User ${id} demoted from admin by ${requestingAdminId}`);
    res.json({ success: true });
  } catch (error) {
    console.error("[Admin] Failed to demote user:", error);
    res.status(500).json({ error: "Failed to demote user" });
  }
});

/**
 * POST /api/admin/users/:id/impersonate
 * Generate impersonation token for a user
 */
router.post("/users/:id/impersonate", sensitiveLimiter, (req: Request, res: Response) => {
  try {
    const { id } = req.params;
    const adminId = req.user?.id;

    if (!adminId) {
      return res.status(401).json({ error: "Not authenticated" });
    }

    const result = createImpersonationToken(id, adminId);

    if (!result) {
      return res.status(404).json({ error: "User not found" });
    }

    res.json(result);
  } catch (error) {
    console.error("[Admin] Failed to create impersonation token:", error);
    res.status(500).json({ error: "Failed to create impersonation token" });
  }
});

// =============================================================================
// Device Management
// =============================================================================

/**
 * GET /api/admin/devices
 * List all devices with pagination and filters
 */
router.get("/devices", (req: Request, res: Response) => {
  try {
    const page = parseInt(req.query.page as string) || 1;
    const pageSize = Math.min(parseInt(req.query.pageSize as string) || 20, 100);
    const search = req.query.search as string | undefined;
    const onlineOnly = req.query.onlineOnly === "true";

    const result = getAllDevices(page, pageSize, search, onlineOnly);
    res.json(result);
  } catch (error) {
    console.error("[Admin] Failed to get devices:", error);
    res.status(500).json({ error: "Failed to get devices" });
  }
});

/**
 * GET /api/admin/devices/:id
 * Get detailed device information
 */
router.get("/devices/:id", (req: Request, res: Response) => {
  try {
    const { id } = req.params;
    const device = getDeviceById(id);

    if (!device) {
      return res.status(404).json({ error: "Device not found" });
    }

    res.json(device);
  } catch (error) {
    console.error("[Admin] Failed to get device:", error);
    res.status(500).json({ error: "Failed to get device" });
  }
});

/**
 * DELETE /api/admin/devices/:id
 * Force delete a device
 */
router.delete("/devices/:id", sensitiveLimiter, (req: Request, res: Response) => {
  try {
    const { id } = req.params;
    const result = forceDeleteDevice(id);

    if (!result.success) {
      return res.status(400).json({ error: result.error });
    }

    console.log(`[Admin] Device ${id} deleted by ${req.user?.id}`);
    res.json({ success: true });
  } catch (error) {
    console.error("[Admin] Failed to delete device:", error);
    res.status(500).json({ error: "Failed to delete device" });
  }
});

/**
 * POST /api/admin/devices/:id/disconnect
 * Force disconnect a device's WebSocket connection
 * Note: This requires access to the DeviceRelay instance
 */
router.post("/devices/:id/disconnect", sensitiveLimiter, (req: Request, res: Response) => {
  // This will be connected to DeviceRelay in server.ts
  const disconnectDevice = (req as Request & { disconnectDevice?: (id: string) => boolean }).disconnectDevice;

  if (!disconnectDevice) {
    return res.status(501).json({ error: "Disconnect not available" });
  }

  try {
    const { id } = req.params;
    const success = disconnectDevice(id);

    if (!success) {
      return res.status(404).json({ error: "Device not connected" });
    }

    console.log(`[Admin] Device ${id} disconnected by ${req.user?.id}`);
    res.json({ success: true });
  } catch (error) {
    console.error("[Admin] Failed to disconnect device:", error);
    res.status(500).json({ error: "Failed to disconnect device" });
  }
});

// =============================================================================
// Session Management
// =============================================================================

/**
 * GET /api/admin/sessions
 * List all sessions with pagination
 */
router.get("/sessions", (req: Request, res: Response) => {
  try {
    const page = parseInt(req.query.page as string) || 1;
    const pageSize = Math.min(parseInt(req.query.pageSize as string) || 20, 100);
    const userId = req.query.userId as string | undefined;

    const result = getAllSessions(page, pageSize, userId);
    res.json(result);
  } catch (error) {
    console.error("[Admin] Failed to get sessions:", error);
    res.status(500).json({ error: "Failed to get sessions" });
  }
});

/**
 * DELETE /api/admin/sessions/:id
 * Revoke any session
 */
router.delete("/sessions/:id", sensitiveLimiter, (req: Request, res: Response) => {
  try {
    const { id } = req.params;
    const result = revokeAnySession(id);

    if (!result.success) {
      return res.status(400).json({ error: result.error });
    }

    console.log(`[Admin] Session ${id} revoked by ${req.user?.id}`);
    res.json({ success: true });
  } catch (error) {
    console.error("[Admin] Failed to revoke session:", error);
    res.status(500).json({ error: "Failed to revoke session" });
  }
});

// =============================================================================
// Real-time Connections
// =============================================================================

/**
 * GET /api/admin/connections
 * Get real-time WebSocket connection information
 * Note: This requires access to DeviceRelay and ClientProxy instances
 */
router.get("/connections", (req: Request, res: Response) => {
  const getConnectionStats = (req as Request & { getConnectionStats?: () => unknown }).getConnectionStats;

  if (!getConnectionStats) {
    return res.status(501).json({ error: "Connection stats not available" });
  }

  try {
    const stats = getConnectionStats();
    res.json(stats);
  } catch (error) {
    console.error("[Admin] Failed to get connection stats:", error);
    res.status(500).json({ error: "Failed to get connection stats" });
  }
});

export default router;

