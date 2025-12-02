import { ReactNode } from "react";

export interface WizardStep {
  id: string;
  title: string;
  icon: ReactNode;
}

export interface PairingData {
  deviceId: string;
  token: string;
  url: string;
  expiresIn: number;
}

