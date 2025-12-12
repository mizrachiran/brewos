/**
 * Firmware Update Management
 *
 * Handles version checking, comparison, and update channel management.
 */

import { formatDate as formatDateUtil } from "./date";

export type UpdateChannel = "stable" | "beta" | "dev";

export interface VersionInfo {
  version: string;
  channel: UpdateChannel;
  releaseDate: string;
  releaseNotes?: string;
  downloadUrl?: string;
  isPrerelease: boolean;
  // Additional info for dev/beta builds
  commitSha?: string;      // First 7 chars of commit SHA
  assetUpdatedAt?: string; // When the assets were last updated (more accurate for dev builds)
}

export interface UpdateCheckResult {
  stable: VersionInfo | null;
  beta: VersionInfo | null;
  dev: VersionInfo | null;
  currentVersion: string;
  hasStableUpdate: boolean;
  hasBetaUpdate: boolean;
  hasDevUpdate: boolean;
}

// GitHub repository for fetching releases
const GITHUB_REPO = "mizrachiran/brewos";

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
  const partsA = va.prerelease!.split(".");
  const partsB = vb.prerelease!.split(".");

  for (let i = 0; i < Math.max(partsA.length, partsB.length); i++) {
    const partA = partsA[i] || "";
    const partB = partsB[i] || "";

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
export function getVersionDisplay(version: string): {
  text: string;
  badge: "stable" | "beta" | "rc" | "alpha" | null;
} {
  const parsed = parseVersion(version);

  if (!parsed.prerelease) {
    return { text: version, badge: "stable" };
  }

  if (parsed.prerelease.startsWith("beta")) {
    return { text: version, badge: "beta" };
  }

  if (parsed.prerelease.startsWith("rc")) {
    return { text: version, badge: "rc" };
  }

  if (parsed.prerelease.startsWith("alpha")) {
    return { text: version, badge: "alpha" };
  }

  return { text: version, badge: null };
}

interface GitHubAsset {
  name: string;
  updated_at: string;
}

interface GitHubRelease {
  tag_name: string;
  prerelease: boolean;
  published_at: string;
  body: string;
  html_url: string;
  target_commitish?: string;
  assets?: GitHubAsset[];
}

/**
 * Fetch releases from GitHub API
 */
async function fetchGitHubReleases(): Promise<VersionInfo[]> {
  try {
    const response = await fetch(
      `https://api.github.com/repos/${GITHUB_REPO}/releases`,
      {
        headers: {
          Accept: "application/vnd.github.v3+json",
        },
      }
    );

    if (!response.ok) {
      throw new Error(`GitHub API error: ${response.status}`);
    }

    const releases: GitHubRelease[] = await response.json();

    return releases.map((release) => {
      const tagName = release.tag_name;
      const isDev = tagName === "dev-latest";
      const isBeta = release.prerelease && !isDev;

      // For dev builds, use the most recent asset update time (when CI uploaded new binaries)
      // This is more accurate than published_at which is when the release was first created
      let effectiveDate = release.published_at;
      if ((isDev || isBeta) && release.assets && release.assets.length > 0) {
        // Find the most recently updated asset
        const latestAsset = release.assets.reduce((latest, asset) => {
          return new Date(asset.updated_at) > new Date(latest.updated_at) ? asset : latest;
        });
        effectiveDate = latestAsset.updated_at;
      }

      // Extract commit SHA from release notes if present (our CI adds it)
      // Format in release notes: "Commit: abc1234" or "Build: abc1234"
      let commitSha: string | undefined;
      if (release.body) {
        const commitMatch = release.body.match(/(?:Commit|Build|SHA):\s*([a-f0-9]{7,40})/i);
        if (commitMatch) {
          commitSha = commitMatch[1].substring(0, 7);
        }
      }

      return {
        version: isDev ? "dev-latest" : tagName.replace(/^v/, ""),
        channel: isDev ? "dev" : release.prerelease ? "beta" : "stable",
        releaseDate: effectiveDate,
        releaseNotes: release.body,
        downloadUrl: release.html_url,
        isPrerelease: release.prerelease || isDev,
        commitSha,
        assetUpdatedAt: effectiveDate,
      };
    });
  } catch (error) {
    console.error("Failed to fetch GitHub releases:", error);
    return [];
  }
}

/**
 * Mock version data for development/testing
 * Only used when __ENVIRONMENT__ is "development"
 */
function getMockVersions(): VersionInfo[] {
  const repoUrl = `https://github.com/${GITHUB_REPO}`;
  return [
    {
      version: "1.0.0",
      channel: "stable",
      releaseDate: "2024-11-15",
      releaseNotes:
        "## Stable Release\n- First stable release\n- All features tested and verified",
      downloadUrl: `${repoUrl}/releases/tag/v1.0.0`,
      isPrerelease: false,
    },
    {
      version: "1.1.0-beta.2",
      channel: "beta",
      releaseDate: "2024-12-01",
      releaseNotes:
        "## Beta Release\n- New cloud integration\n- Push notifications\n- UI improvements\n\n⚠️ This is a beta version for testing.\n\nCommit: a1b2c3d",
      downloadUrl: `${repoUrl}/releases/tag/v1.1.0-beta.2`,
      isPrerelease: true,
      commitSha: "a1b2c3d",
      assetUpdatedAt: "2024-12-01T10:30:00Z",
    },
    {
      version: "1.1.0-beta.1",
      channel: "beta",
      releaseDate: "2024-11-25",
      releaseNotes: "## Beta Release\n- Initial beta with new features\n\nCommit: e4f5g6h",
      downloadUrl: `${repoUrl}/releases/tag/v1.1.0-beta.1`,
      isPrerelease: true,
      commitSha: "e4f5g6h",
      assetUpdatedAt: "2024-11-25T14:20:00Z",
    },
    {
      version: "dev-latest",
      channel: "dev",
      releaseDate: new Date().toISOString(),
      releaseNotes:
        "## Dev Build\n- Latest from main branch\n- For development testing only\n\nCommit: abc1234",
      downloadUrl: `${repoUrl}/releases/tag/dev-latest`,
      isPrerelease: true,
      commitSha: "abc1234",
      assetUpdatedAt: new Date().toISOString(),
    },
  ];
}

/**
 * Check if we should use mock data
 * Only use mock data in development mode (local dev server)
 */
function shouldUseMockData(): boolean {
  // __ENVIRONMENT__ is set at build time by vite.config.ts
  // "development" = local dev server, "staging"/"production" = real builds
  return __ENVIRONMENT__ === "development";
}

/**
 * Check for available updates
 * Automatically uses real GitHub API in production/staging, mock data in development
 */
export async function checkForUpdates(
  currentVersion: string
): Promise<UpdateCheckResult> {
  // Use mock data in development, real GitHub API in production/staging
  const useMock = shouldUseMockData();
  const releases = useMock ? getMockVersions() : await fetchGitHubReleases();

  if (!useMock) {
    console.log(
      "[Updates] Fetched releases from GitHub:",
      releases.map((r) => `${r.version} (${r.channel})`)
    );
  }

  // Find latest stable version
  const stableReleases = releases
    .filter((r) => r.channel === "stable")
    .sort((a, b) => compareVersions(b.version, a.version));

  const latestStable = stableReleases[0] || null;

  // Find latest beta version (including RCs, excluding dev)
  const betaReleases = releases
    .filter((r) => r.channel === "beta")
    .sort((a, b) => compareVersions(b.version, a.version));

  const latestBeta = betaReleases[0] || null;

  // Find dev release (dev-latest tag)
  const latestDev = releases.find((r) => r.channel === "dev") || null;

  // Check if updates are available
  const hasStableUpdate = latestStable
    ? compareVersions(latestStable.version, currentVersion) > 0
    : false;

  // Beta update available if beta is newer than current (even if current is stable)
  const hasBetaUpdate = latestBeta
    ? compareVersions(latestBeta.version, currentVersion) > 0
    : false;

  // Dev is always considered "available" since it's the latest from main
  const hasDevUpdate = latestDev !== null;

  return {
    stable: latestStable,
    beta: latestBeta,
    dev: latestDev,
    currentVersion,
    hasStableUpdate,
    hasBetaUpdate,
    hasDevUpdate,
  };
}

/**
 * Get user's preferred update channel from localStorage
 */
export function getUpdateChannel(): UpdateChannel {
  const stored = localStorage.getItem("brewos-update-channel");
  if (stored === "beta" || stored === "stable" || stored === "dev") {
    return stored;
  }
  return "stable"; // Default to stable
}

/**
 * Set user's preferred update channel
 */
export function setUpdateChannel(channel: UpdateChannel): void {
  localStorage.setItem("brewos-update-channel", channel);
}

/**
 * Format release date
 */
export function formatReleaseDate(dateString: string): string {
  return formatDateUtil(dateString, { dateStyle: "medium" });
}
