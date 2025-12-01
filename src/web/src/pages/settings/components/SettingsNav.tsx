import React, { useState, useEffect, useRef } from 'react';
import { cn } from '@/lib/utils';
import { ChevronDown, Check } from 'lucide-react';
import { SettingsTab, TabConfig } from '../constants/tabs';

interface SettingsNavProps {
  tabs: TabConfig[];
  activeTab: SettingsTab;
  onTabChange: (tab: SettingsTab) => void;
}

export function SettingsNav({ tabs, activeTab, onTabChange }: SettingsNavProps) {
  const [dropdownOpen, setDropdownOpen] = useState(false);
  const dropdownRef = useRef<HTMLDivElement>(null);
  
  const currentTab = tabs.find(t => t.id === activeTab);
  const CurrentIcon = currentTab?.icon;

  // Close dropdown when clicking outside
  useEffect(() => {
    const handleClickOutside = (event: MouseEvent) => {
      if (dropdownRef.current && !dropdownRef.current.contains(event.target as Node)) {
        setDropdownOpen(false);
      }
    };
    document.addEventListener('mousedown', handleClickOutside);
    return () => document.removeEventListener('mousedown', handleClickOutside);
  }, []);

  return (
    <div>
      {/* Mobile/Tablet: Custom dropdown with icons */}
      <div className="lg:hidden" ref={dropdownRef}>
        <div className="relative">
          {/* Dropdown trigger */}
          <button
            onClick={() => setDropdownOpen(!dropdownOpen)}
            className="w-full flex items-center justify-between settings-tab rounded-xl px-4 py-3.5 text-base font-medium text-theme focus:outline-none focus:ring-2 focus:ring-accent focus:border-transparent shadow-sm"
          >
            <div className="flex items-center gap-3">
              {CurrentIcon && <CurrentIcon className="w-5 h-5 text-accent" />}
              <span>{currentTab?.label}</span>
            </div>
            <ChevronDown className={cn(
              "w-5 h-5 text-theme-muted transition-transform duration-200",
              dropdownOpen && "rotate-180"
            )} />
          </button>
          
          {/* Dropdown menu */}
          {dropdownOpen && (
            <div className="absolute z-50 mt-2 w-full dropdown-menu rounded-xl overflow-hidden">
              <div className="py-1 max-h-80 overflow-y-auto">
                {tabs.map((tab) => {
                  const Icon = tab.icon;
                  const isActive = activeTab === tab.id;
                  return (
                    <button
                      key={tab.id}
                      onClick={() => {
                        onTabChange(tab.id);
                        setDropdownOpen(false);
                      }}
                      className={cn(
                        'w-full flex items-center gap-3 px-4 py-3 text-left transition-colors',
                        isActive
                          ? 'dropdown-item-active'
                          : 'dropdown-item'
                      )}
                    >
                      <Icon className={cn(
                        "w-5 h-5 flex-shrink-0",
                        isActive ? "" : "text-accent"
                      )} />
                      <span className="font-medium">{tab.label}</span>
                      {isActive && (
                        <Check className="w-4 h-4 ml-auto" />
                      )}
                    </button>
                  );
                })}
              </div>
            </div>
          )}
        </div>
      </div>

      {/* Desktop: Horizontal scrollable tabs */}
      <div className="hidden lg:block settings-tab rounded-xl p-2">
        <div className="flex gap-1 overflow-x-auto scrollbar-hide">
          {tabs.map((tab) => {
            const Icon = tab.icon;
            const isActive = activeTab === tab.id;
            return (
              <button
                key={tab.id}
                onClick={() => onTabChange(tab.id)}
                className={cn(
                  'flex items-center gap-2 px-4 py-2 rounded-lg text-sm font-medium whitespace-nowrap transition-all',
                  isActive
                    ? 'settings-tab-active'
                    : 'settings-tab-inactive'
                )}
                title={tab.label}
              >
                <Icon className="w-4 h-4 flex-shrink-0" />
                <span>{tab.label}</span>
              </button>
            );
          })}
        </div>
      </div>
    </div>
  );
}

