import { Request, Response, NextFunction } from 'express';
import jwt from 'jsonwebtoken';
import { ensureProfile } from '../services/device.js';

// Supabase JWT secret (from project settings)
const SUPABASE_JWT_SECRET = process.env.SUPABASE_JWT_SECRET || '';

// Extend Express Request type
declare global {
  namespace Express {
    interface Request {
      user?: {
        id: string;
        email: string;
      };
    }
  }
}

interface SupabaseJWTPayload {
  sub: string;           // User ID
  email?: string;
  user_metadata?: {
    full_name?: string;
    name?: string;
    avatar_url?: string;
  };
  iat: number;
  exp: number;
}

/**
 * Middleware to verify Supabase JWT token
 */
export function supabaseAuthMiddleware(
  req: Request,
  res: Response,
  next: NextFunction
): void {
  const authHeader = req.headers.authorization;

  if (!authHeader?.startsWith('Bearer ')) {
    res.status(401).json({ error: 'Missing authorization header' });
    return;
  }

  const token = authHeader.slice(7);

  if (!SUPABASE_JWT_SECRET) {
    console.error('[Auth] SUPABASE_JWT_SECRET not configured');
    res.status(500).json({ error: 'Auth not configured' });
    return;
  }

  try {
    // Verify the JWT
    const payload = jwt.verify(token, SUPABASE_JWT_SECRET) as SupabaseJWTPayload;

    // Attach user to request
    req.user = {
      id: payload.sub,
      email: payload.email || '',
    };

    // Ensure profile exists in local DB
    ensureProfile(
      payload.sub,
      payload.email,
      payload.user_metadata?.full_name || payload.user_metadata?.name,
      payload.user_metadata?.avatar_url
    );

    next();
  } catch (error) {
    console.error('[Auth] Token verification failed:', error);
    res.status(401).json({ error: 'Invalid or expired token' });
  }
}

/**
 * Optional auth - doesn't require auth but attaches user if present
 */
export function optionalAuthMiddleware(
  req: Request,
  _res: Response,
  next: NextFunction
): void {
  const authHeader = req.headers.authorization;

  if (authHeader?.startsWith('Bearer ') && SUPABASE_JWT_SECRET) {
    const token = authHeader.slice(7);

    try {
      const payload = jwt.verify(token, SUPABASE_JWT_SECRET) as SupabaseJWTPayload;

      req.user = {
        id: payload.sub,
        email: payload.email || '',
      };
    } catch {
      // Ignore errors - just continue without user
    }
  }

  next();
}
