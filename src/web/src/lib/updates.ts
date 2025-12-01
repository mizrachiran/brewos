/**
 * Firmware Update Management
 * 
 * Handles version checking, comparison, and update channel management.
 */

export type UpdateChannel = 'stable' | 'beta';

export interface VersionInfo {
  version: string;
  channel: UpdateChannel;
  releaseDate: string;
  releaseNotes?: string;
  downloadUrl?: string;
  isPrerelease: boolean;
}

export interface UpdateCheckResult {
  stable: VersionInfo | null;
  beta: VersionInfo | null;
  currentVersion: string;
  hasStableUpdate: boolean;
  hasBetaUpdate: boolean;
}

// GitHub repository for fetching releases (configure this)
const GITHUB_REPO = 'brewos/brewos'; // Change to your repo

/**
 * Parse semantic version string
 * @example "1.2.3-beta.1" -> { major: 1, minor: 2, patch: 3, prerelease: "beta.1" }
 */
export function parseVersion(version: string): {
  major: number;
  minor: number;
  patch: number;
  prerelease: string | null;
} {
  const match = version.match(/^v?(\d+)\.(\d+)\.(\d+)(?:-(.+))?$/);
  if (!match) {
    return { major: 0, minor: 0, patch: 0, prerelease: null };
  }
  return {
    major: parseInt(match[1], 10),
    minor: parseInt(match[2], 10),
    patch: parseInt(match[3], 10),
    prerelease: match[4] || null,
  };
}

/**
 * Compare two semantic versions
 * @returns -1 if a < b, 0 if a == b, 1 if a > b
 */
export function compareVersions(a: string, b: string): number {
  const va = parseVersion(a);
  const vb = parseVersion(b);

  // Compare major.minor.patch
  if (va.major !== vb.major) return va.major > vb.major ? 1 : -1;
  if (va.minor !== vb.minor) return va.minor > vb.minor ? 1 : -1;
  if (va.patch !== vb.patch) return va.patch > vb.patch ? 1 : -1;

  // Both are same base version, compare prerelease
  // No prerelease > has prerelease (stable > beta)
  if (!va.prerelease && vb.prerelease) return 1;
  if (va.prerelease && !vb.prerelease) return -1;
  if (!va.prerelease && !vb.prerelease) return 0;

  // Both have prerelease, compare alphabetically then numerically
  // beta.1 < beta.2, alpha < beta < rc
  const partsA = va.prerelease!.split('.');
  const partsB = vb.prerelease!.split('.');

  for (let i = 0; i < Math.max(partsA.length, partsB.length); i++) {
    const partA = partsA[i] || '';
    const partB = partsB[i] || '';

    const numA = parseInt(partA, 10);
    const numB = parseInt(partB, 10);

    if (!isNaN(numA) && !isNaN(numB)) {
      if (numA !== numB) return numA > numB ? 1 : -1;
    } else {
      if (partA !== partB) return partA > partB ? 1 : -1;
    }
  }

  return 0;
}

/**
 * Check if version is a prerelease (beta, alpha, rc)
 */
export function isPrerelease(version: string): boolean {
  const parsed = parseVersion(version);
  return parsed.prerelease !== null;
}

/**
 * Get version display string with badge
 */
export function getVersionDisplay(version: string): { text: string; badge: 'stable' | 'beta' | 'rc' | 'alpha' | null } {
  const parsed = parseVersion(version);
  
  if (!parsed.prerelease) {
    return { text: version, badge: 'stable' };
  }

  if (parsed.prerelease.startsWith('beta')) {
    return { text: version, badge: 'beta' };
  }

  if (parsed.prerelease.startsWith('rc')) {
    return { text: version, badge: 'rc' };
  }

  if (parsed.prerelease.startsWith('alpha')) {
    return { text: version, badge: 'alpha' };
  }

  return { text: version, badge: null };
}

/**
 * Fetch releases from GitHub API
 */
async function fetchGitHubReleases(): Promise<VersionInfo[]> {
  try {
    const response = await fetch(`https://api.github.com/repos/${GITHUB_REPO}/releases`, {
      headers: {
        'Accept': 'application/vnd.github.v3+json',
      },
    });

    if (!response.ok) {
      throw new Error(`GitHub API error: ${response.status}`);
    }

    const releases = await response.json();
    
    return releases.map((release: any) => ({
      version: release.tag_name.replace(/^v/, ''),
      channel: release.prerelease ? 'beta' : 'stable',
      releaseDate: release.published_at,
      releaseNotes: release.body,
      downloadUrl: release.html_url,
      isPrerelease: release.prerelease,
    }));
  } catch (error) {
    console.error('Failed to fetch GitHub releases:', error);
    return [];
  }
}

/**
 * Mock version data for development/testing
 * In production, this would be replaced with actual GitHub releases
 */
function getMockVersions(): VersionInfo[] {
  return [
    {
      version: '1.0.0',
      channel: 'stable',
      releaseDate: '2024-11-15',
      releaseNotes: '## Stable Release\n- First stable release\n- All features tested and verified',
      downloadUrl: '#',
      isPrerelease: false,
    },
    {
      version: '1.1.0-beta.2',
      channel: 'beta',
      releaseDate: '2024-12-01',
      releaseNotes: '## Beta Release\n- New cloud integration\n- Push notifications\n- UI improvements\n\n⚠️ This is a beta version for testing.',
      downloadUrl: '#',
      isPrerelease: true,
    },
    {
      version: '1.1.0-beta.1',
      channel: 'beta',
      releaseDate: '2024-11-25',
      releaseNotes: '## Beta Release\n- Initial beta with new features',
      downloadUrl: '#',
      isPrerelease: true,
    },
  ];
}

/**
 * Check for available updates
 */
export async function checkForUpdates(currentVersion: string, useMockData = true): Promise<UpdateCheckResult> {
  // Use mock data in development, real GitHub API in production
  const releases = useMockData ? getMockVersions() : await fetchGitHubReleases();
  
  // Find latest stable version
  const stableReleases = releases
    .filter(r => !r.isPrerelease)
    .sort((a, b) => compareVersions(b.version, a.version));
  
  const latestStable = stableReleases[0] || null;
  
  // Find latest beta version (including RCs)
  const betaReleases = releases
    .filter(r => r.isPrerelease)
    .sort((a, b) => compareVersions(b.version, a.version));
  
  const latestBeta = betaReleases[0] || null;
  
  // Check if updates are available
  const hasStableUpdate = latestStable 
    ? compareVersions(latestStable.version, currentVersion) > 0 
    : false;
  
  // Beta update available if beta is newer than current (even if current is stable)
  const hasBetaUpdate = latestBeta 
    ? compareVersions(latestBeta.version, currentVersion) > 0 
    : false;
  
  return {
    stable: latestStable,
    beta: latestBeta,
    currentVersion,
    hasStableUpdate,
    hasBetaUpdate,
  };
}

/**
 * Get user's preferred update channel from localStorage
 */
export function getUpdateChannel(): UpdateChannel {
  const stored = localStorage.getItem('brewos-update-channel');
  if (stored === 'beta' || stored === 'stable') {
    return stored;
  }
  return 'stable'; // Default to stable
}

/**
 * Set user's preferred update channel
 */
export function setUpdateChannel(channel: UpdateChannel): void {
  localStorage.setItem('brewos-update-channel', channel);
}

/**
 * Format release date
 */
export function formatReleaseDate(dateString: string): string {
  const date = new Date(dateString);
  return date.toLocaleDateString(undefined, {
    year: 'numeric',
    month: 'short',
    day: 'numeric',
  });
}

