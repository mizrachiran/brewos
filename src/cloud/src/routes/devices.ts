import { Router, Request } from "express";
import rateLimit from "express-rate-limit";
import { sessionAuthMiddleware } from "../middleware/auth.js";
import {
  claimDevice,
  verifyClaimToken,
  createClaimToken,
  getUserDevices,
  getDevice,
  removeDevice,
  renameDevice,
  updateDeviceMachineInfo,
  userOwnsDevice,
  getDeviceUsers,
  revokeUserAccess,
  getDeviceUserCount,
} from "../services/device.js";

const router = Router();

// ============================================================================
// Rate Limiters
// ============================================================================

// General rate limiter applied to ALL routes in this router
// 100 requests per 15 minutes per IP (first line of defense)
const generalLimiter = rateLimit({
  windowMs: 15 * 60 * 1000,
  max: 100,
  standardHeaders: true,
  legacyHeaders: false,
  message: { error: "Too many requests, please try again later" },
});

// Strict limiter for sensitive operations (claiming, ESP32 registration)
// 5 requests per minute per IP
const strictLimiter = rateLimit({
  windowMs: 60 * 1000,
  max: 5,
  standardHeaders: true,
  legacyHeaders: false,
  message: { error: "Too many requests, please try again later" },
});

// Write limiter for mutations (30 requests per 15 minutes)
const writeLimiter = rateLimit({
  windowMs: 15 * 60 * 1000,
  max: 30,
  standardHeaders: true,
  legacyHeaders: false,
  message: { error: "Too many requests, please try again later" },
});

// Apply general rate limiting to ALL routes in this router
router.use(generalLimiter);

// ============================================================================
// Routes
// ============================================================================

/**
 * POST /api/devices/register-claim
 * Called by ESP32 to register a claim token
 * No auth required (the token itself is the auth)
 * Extra strict rate limiting
 */
router.post("/register-claim", strictLimiter, (req, res) => {
  try {
    const { deviceId, token } = req.body;

    if (!deviceId || !token) {
      return res.status(400).json({ error: "Missing deviceId or token" });
    }

    // Validate device ID format
    if (!deviceId.match(/^BRW-[A-F0-9]{8}$/)) {
      return res.status(400).json({ error: "Invalid device ID format" });
    }

    createClaimToken(deviceId, token);

    res.json({ success: true });
  } catch (error) {
    console.error("Failed to register claim token:", error);
    res.status(500).json({ error: "Failed to register claim token" });
  }
});

/**
 * POST /api/devices/claim
 * Claim a device using QR code token
 * Extra strict rate limiting to prevent brute-force
 */
router.post("/claim", strictLimiter, sessionAuthMiddleware, (req, res) => {
  try {
    const { deviceId, token, name } = req.body;
    const userId = req.user!.id;

    if (!deviceId || !token) {
      return res.status(400).json({ error: "Missing deviceId or token" });
    }

    // Verify the claim token
    const isValid = verifyClaimToken(deviceId, token);
    if (!isValid) {
      return res.status(400).json({ error: "Invalid or expired claim token" });
    }

    // Claim the device
    const device = claimDevice(deviceId, userId, name);

    // Get the user-specific name from user_devices
    const userDevices = getUserDevices(userId);
    const userDevice = userDevices.find((d) => d.id === deviceId);

    res.json({
      success: true,
      device: {
        id: device.id,
        name: userDevice?.user_name || name || "My BrewOS",
        claimedAt: userDevice?.user_claimed_at || device.claimed_at,
      },
    });
  } catch (error) {
    const message =
      error instanceof Error ? error.message : "Failed to claim device";
    res.status(400).json({ error: message });
  }
});

/**
 * GET /api/devices
 * List user's devices
 */
router.get("/", sessionAuthMiddleware, (req, res) => {
  try {
    const devices = getUserDevices(req.user!.id);

    res.json({
      devices: devices.map((d) => ({
        id: d.id,
        name: d.user_name,
        machineBrand: d.machine_brand,
        machineModel: d.machine_model,
        isOnline: !!d.is_online,
        lastSeen: d.last_seen_at,
        firmwareVersion: d.firmware_version,
        machineType: d.machine_type,
        claimedAt: d.user_claimed_at,
        userCount: getDeviceUserCount(d.id),
      })),
    });
  } catch (error) {
    res.status(500).json({ error: "Failed to get devices" });
  }
});

/**
 * GET /api/devices/:id
 * Get a specific device
 */
router.get("/:id", sessionAuthMiddleware, (req, res) => {
  try {
    const { id } = req.params;

    if (!userOwnsDevice(req.user!.id, id)) {
      return res.status(404).json({ error: "Device not found" });
    }

    const device = getDevice(id, req.user!.id);
    if (!device) {
      return res.status(404).json({ error: "Device not found" });
    }

    const userDevices = getUserDevices(req.user!.id);
    const userDevice = userDevices.find((d) => d.id === id);

    res.json({
      device: {
        id: device.id,
        name: (device as any).user_name || device.name,
        machineBrand: device.machine_brand,
        machineModel: device.machine_model,
        isOnline: !!device.is_online,
        lastSeen: device.last_seen_at,
        firmwareVersion: device.firmware_version,
        machineType: device.machine_type,
        claimedAt: userDevice?.user_claimed_at || device.claimed_at,
      },
    });
  } catch (error) {
    res.status(500).json({ error: "Failed to get device" });
  }
});

/**
 * PATCH /api/devices/:id
 * Update device (name, brand, model)
 */
router.patch("/:id", writeLimiter, sessionAuthMiddleware, (req, res) => {
  try {
    const { id } = req.params;
    const { name, brand, model } = req.body;

    if (!name && !brand && !model) {
      return res.status(400).json({ error: "Missing name, brand, or model" });
    }

    updateDeviceMachineInfo(id, req.user!.id, { name, brand, model });

    res.json({ success: true });
  } catch (error) {
    res.status(500).json({ error: "Failed to update device" });
  }
});

/**
 * GET /api/devices/:id/users
 * Get all users who have access to a device
 */
router.get("/:id/users", sessionAuthMiddleware, (req, res) => {
  try {
    const { id } = req.params;

    if (!userOwnsDevice(req.user!.id, id)) {
      return res.status(404).json({ error: "Device not found" });
    }

    const users = getDeviceUsers(id);

    res.json({
      users: users.map((u) => ({
        userId: u.user_id,
        email: u.email,
        displayName: u.display_name,
        avatarUrl: u.avatar_url,
        claimedAt: u.claimed_at,
      })),
    });
  } catch (error) {
    res.status(500).json({ error: "Failed to get device users" });
  }
});

/**
 * DELETE /api/devices/:id/users/:userId
 * Revoke a user's access to a device (mutual removal)
 */
router.delete(
  "/:id/users/:userId",
  writeLimiter,
  sessionAuthMiddleware,
  (req, res) => {
    try {
      const { id, userId } = req.params;
      const removingUserId = req.user!.id;

      revokeUserAccess(id, userId, removingUserId);

      res.json({ success: true });
    } catch (error) {
      const message =
        error instanceof Error ? error.message : "Failed to revoke user access";
      const status = message.includes("not have access")
        ? 404
        : message.includes("Cannot remove yourself")
        ? 400
        : 500;
      res.status(status).json({ error: message });
    }
  }
);

/**
 * DELETE /api/devices/:id
 * Remove device from account
 */
router.delete("/:id", writeLimiter, sessionAuthMiddleware, (req, res) => {
  try {
    const { id } = req.params;

    removeDevice(id, req.user!.id);

    res.json({ success: true });
  } catch (error) {
    res.status(500).json({ error: "Failed to remove device" });
  }
});

export default router;
