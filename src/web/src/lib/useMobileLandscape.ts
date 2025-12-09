import { useState, useEffect } from "react";

/**
 * Hook to detect mobile landscape orientation.
 * Returns true when in landscape with limited height (≤500px).
 */
export function useMobileLandscape() {
  const [isMobileLandscape, setIsMobileLandscape] = useState(() => {
    if (typeof window === "undefined") return false;
    return window.innerHeight <= 500 && window.innerWidth > window.innerHeight;
  });

  useEffect(() => {
    const landscapeQuery = window.matchMedia(
      "(orientation: landscape) and (max-height: 500px)"
    );
    const handleChange = (e: MediaQueryListEvent | MediaQueryList) =>
      setIsMobileLandscape(e.matches);
    handleChange(landscapeQuery);
    landscapeQuery.addEventListener("change", handleChange);
    return () => landscapeQuery.removeEventListener("change", handleChange);
  }, []);

  return isMobileLandscape;
}

