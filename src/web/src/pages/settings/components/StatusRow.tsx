import React from 'react';

interface StatusRowProps {
  label: string;
  value: React.ReactNode;
  mono?: boolean;
}

export function StatusRow({ label, value, mono }: StatusRowProps) {
  return (
    <div className="flex items-center justify-between py-2 border-b border-cream-200 last:border-0">
      <span className="text-sm text-coffee-500">{label}</span>
      <span className={`text-sm font-medium text-coffee-900 ${mono ? 'font-mono' : ''}`}>
        {value}
      </span>
    </div>
  );
}

