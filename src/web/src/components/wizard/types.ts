import { ReactElement } from "react";

export interface WizardStep {
  id: string;
  title: string;
  icon: ReactElement;
}

export interface PairingData {
  deviceId: string;
  token: string;
  url: string;
  expiresIn: number;
  manualCode?: string; // Short code like "X6ST-AP3G" for manual entry
}

