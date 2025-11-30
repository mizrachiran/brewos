import { Router } from 'express';
import { supabaseAuthMiddleware } from '../middleware/auth.js';
import {
  claimDevice,
  verifyClaimToken,
  createClaimToken,
  getUserDevices,
  getDevice,
  removeDevice,
  renameDevice,
  userOwnsDevice,
} from '../services/device.js';

const router = Router();

/**
 * POST /api/devices/register-claim
 * Called by ESP32 to register a claim token
 * No auth required (the token itself is the auth)
 */
router.post('/register-claim', (req, res) => {
  try {
    const { deviceId, token } = req.body;

    if (!deviceId || !token) {
      return res.status(400).json({ error: 'Missing deviceId or token' });
    }

    // Validate device ID format
    if (!deviceId.match(/^BRW-[A-F0-9]{8}$/)) {
      return res.status(400).json({ error: 'Invalid device ID format' });
    }

    createClaimToken(deviceId, token);

    res.json({ success: true });
  } catch (error) {
    console.error('Failed to register claim token:', error);
    res.status(500).json({ error: 'Failed to register claim token' });
  }
});

/**
 * POST /api/devices/claim
 * Claim a device using QR code token
 */
router.post('/claim', supabaseAuthMiddleware, (req, res) => {
  try {
    const { deviceId, token, name } = req.body;
    const userId = req.user!.id;

    if (!deviceId || !token) {
      return res.status(400).json({ error: 'Missing deviceId or token' });
    }

    // Verify the claim token
    const isValid = verifyClaimToken(deviceId, token);
    if (!isValid) {
      return res.status(400).json({ error: 'Invalid or expired claim token' });
    }

    // Claim the device
    const device = claimDevice(deviceId, userId, name);

    res.json({
      success: true,
      device: {
        id: device.id,
        name: device.name,
        claimedAt: device.claimed_at,
      },
    });
  } catch (error) {
    const message = error instanceof Error ? error.message : 'Failed to claim device';
    res.status(400).json({ error: message });
  }
});

/**
 * GET /api/devices
 * List user's devices
 */
router.get('/', supabaseAuthMiddleware, (req, res) => {
  try {
    const devices = getUserDevices(req.user!.id);

    res.json({
      devices: devices.map(d => ({
        id: d.id,
        name: d.name,
        isOnline: !!d.is_online,
        lastSeen: d.last_seen_at,
        firmwareVersion: d.firmware_version,
        machineType: d.machine_type,
        claimedAt: d.claimed_at,
      })),
    });
  } catch (error) {
    res.status(500).json({ error: 'Failed to get devices' });
  }
});

/**
 * GET /api/devices/:id
 * Get a specific device
 */
router.get('/:id', supabaseAuthMiddleware, (req, res) => {
  try {
    const { id } = req.params;

    // Check ownership
    if (!userOwnsDevice(req.user!.id, id)) {
      return res.status(404).json({ error: 'Device not found' });
    }

    const device = getDevice(id);
    if (!device) {
      return res.status(404).json({ error: 'Device not found' });
    }

    res.json({
      device: {
        id: device.id,
        name: device.name,
        isOnline: !!device.is_online,
        lastSeen: device.last_seen_at,
        firmwareVersion: device.firmware_version,
        machineType: device.machine_type,
        claimedAt: device.claimed_at,
      },
    });
  } catch (error) {
    res.status(500).json({ error: 'Failed to get device' });
  }
});

/**
 * PATCH /api/devices/:id
 * Update device (rename)
 */
router.patch('/:id', supabaseAuthMiddleware, (req, res) => {
  try {
    const { id } = req.params;
    const { name } = req.body;

    if (!name) {
      return res.status(400).json({ error: 'Missing name' });
    }

    renameDevice(id, req.user!.id, name);

    res.json({ success: true });
  } catch (error) {
    res.status(500).json({ error: 'Failed to update device' });
  }
});

/**
 * DELETE /api/devices/:id
 * Remove device from account
 */
router.delete('/:id', supabaseAuthMiddleware, (req, res) => {
  try {
    const { id } = req.params;

    removeDevice(id, req.user!.id);

    res.json({ success: true });
  } catch (error) {
    res.status(500).json({ error: 'Failed to remove device' });
  }
});

export default router;
