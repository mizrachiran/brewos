import type { Meta, StoryObj } from "@storybook/react";
import { Card } from "./Card";
import { Button } from "./Button";
import { ToastProvider } from "./Toast";
import { ConfirmDialog } from "./ConfirmDialog";
import {
  UserX,
  Loader2,
  Calendar,
  X,
  RefreshCw,
  AlertCircle,
  User,
  LogOut,
} from "lucide-react";

const mockUsers = [
  {
    userId: "user-123",
    email: "john@example.com",
    displayName: "John Doe",
    avatarUrl: "https://api.dicebear.com/7.x/avataaars/svg?seed=John",
    claimedAt: new Date(Date.now() - 2 * 24 * 60 * 60 * 1000).toISOString(),
  },
  {
    userId: "user-456",
    email: "jane@example.com",
    displayName: "Jane Smith",
    avatarUrl: "https://api.dicebear.com/7.x/avataaars/svg?seed=Jane",
    claimedAt: new Date(Date.now() - 7 * 24 * 60 * 60 * 1000).toISOString(),
  },
  {
    userId: "user-789",
    email: "bob@example.com",
    displayName: null,
    avatarUrl: null,
    claimedAt: new Date(Date.now() - 30 * 24 * 60 * 60 * 1000).toISOString(),
  },
];

// Since DeviceUsers uses hooks that require mocking, we create a visual-only story
// that shows the different states without the actual component
interface DeviceUserData {
  userId: string;
  email: string | null;
  displayName: string | null;
  avatarUrl: string | null;
  claimedAt: string;
}

interface MockDeviceUsersProps {
  deviceName?: string;
  users?: DeviceUserData[];
  state?: "loading" | "error" | "empty" | "loaded";
  onClose?: () => void;
}

// Pure presentational component for Storybook
function MockDeviceUsersDialog({
  deviceName = "My ECM Synchronika",
  users = mockUsers,
  state = "loaded",
  onClose = () => {},
}: MockDeviceUsersProps) {
  const currentUserId = "user-123";

  const formatDate = (dateString: string) => {
    try {
      const date = new Date(dateString);
      const now = new Date();
      const diffMs = now.getTime() - date.getTime();
      const diffDays = Math.floor(diffMs / (1000 * 60 * 60 * 24));

      if (diffDays === 0) return "Today";
      if (diffDays === 1) return "Yesterday";
      if (diffDays < 7) return `${diffDays} days ago`;
      if (diffDays < 30) {
        const weeks = Math.floor(diffDays / 7);
        return `${weeks} ${weeks === 1 ? "week" : "weeks"} ago`;
      }
      return date.toLocaleDateString(undefined, {
        year: "numeric",
        month: "short",
        day: "numeric",
      });
    } catch {
      return "Unknown";
    }
  };

  const getUserDisplayName = (user: DeviceUserData) =>
    user.displayName || user.email || "Unknown User";

  const isCurrentUser = (userId: string) => userId === currentUserId;

  return (
    <div className="flex items-center justify-center min-h-[400px]">
      <Card className="w-full max-w-md max-h-[85vh] overflow-hidden flex flex-col shadow-2xl">
        {/* Header - cleaner design */}
        <div className="flex items-center justify-between p-4 border-b border-theme/10">
          <div className="flex-1 min-w-0 pr-4">
            <h2 className="text-lg font-semibold text-theme">Device Users</h2>
            <p className="text-sm text-theme-muted truncate">{deviceName}</p>
          </div>
          <button
            onClick={onClose}
            className="w-8 h-8 rounded-lg hover:bg-theme-secondary flex items-center justify-center transition-colors flex-shrink-0"
          >
            <X className="w-4 h-4 text-theme-muted" />
          </button>
        </div>

        {/* Content */}
        <div className="flex-1 overflow-y-auto p-4 space-y-3">
          {state === "loading" && (
            <div className="flex flex-col items-center justify-center py-12">
              <Loader2 className="w-8 h-8 text-accent animate-spin mb-3" />
              <p className="text-sm text-theme-muted">Loading users...</p>
            </div>
          )}

          {state === "error" && (
            <div className="flex flex-col items-center justify-center py-12 text-center">
              <div className="w-12 h-12 rounded-full bg-error-soft flex items-center justify-center mb-3">
                <AlertCircle className="w-6 h-6 text-error" />
              </div>
              <p className="font-medium text-theme mb-1">
                Failed to load users
              </p>
              <p className="text-sm text-theme-muted mb-4">
                Failed to load device users
              </p>
              <Button variant="ghost" size="sm" className="gap-2">
                <RefreshCw className="w-4 h-4" />
                Try Again
              </Button>
            </div>
          )}

          {state === "empty" && (
            <div className="flex flex-col items-center justify-center py-12 text-center">
              <div className="w-16 h-16 rounded-full bg-theme-secondary flex items-center justify-center mb-4">
                <User className="w-8 h-8 text-theme-muted" />
              </div>
              <p className="font-medium text-theme mb-1">No other users</p>
              <p className="text-sm text-theme-muted max-w-xs">
                You're the only one with access to this device. Share the QR
                code to let others add it to their account.
              </p>
            </div>
          )}

          {state === "loaded" &&
            users.map((deviceUser) => (
              <div
                key={deviceUser.userId}
                className="group flex items-center justify-between p-4 rounded-xl bg-theme-secondary/30 border border-theme/10 hover:border-theme/20 hover:bg-theme-secondary/50 transition-all duration-200"
              >
                <div className="flex items-center gap-3 flex-1 min-w-0">
                  {deviceUser.avatarUrl ? (
                    <img
                      src={deviceUser.avatarUrl}
                      alt={getUserDisplayName(deviceUser)}
                      className="w-12 h-12 rounded-full flex-shrink-0 ring-2 ring-theme/10"
                    />
                  ) : (
                    <div className="w-12 h-12 rounded-full bg-gradient-to-br from-accent/20 to-accent/10 flex items-center justify-center flex-shrink-0 ring-2 ring-theme/10">
                      <User className="w-6 h-6 text-accent" />
                    </div>
                  )}
                  <div className="flex-1 min-w-0">
                    <div className="flex items-center gap-2 mb-1">
                      <p className="font-semibold text-sm text-theme truncate">
                        {getUserDisplayName(deviceUser)}
                      </p>
                      {isCurrentUser(deviceUser.userId) && (
                        <span className="text-xs px-2 py-0.5 rounded-full bg-accent/20 text-accent font-medium flex-shrink-0">
                          You
                        </span>
                      )}
                    </div>
                    <div className="flex items-center gap-1.5 text-xs text-theme-muted">
                      <Calendar className="w-3 h-3" />
                      <span>Added {formatDate(deviceUser.claimedAt)}</span>
                    </div>
                  </div>
                </div>
                {isCurrentUser(deviceUser.userId) ? (
                  <button
                    className="ml-2 w-8 h-8 rounded-lg text-theme-muted hover:text-error hover:bg-error-soft flex items-center justify-center transition-colors flex-shrink-0"
                    title="Leave device"
                  >
                    <LogOut className="w-4 h-4" />
                  </button>
                ) : (
                  <button
                    className="ml-2 w-8 h-8 rounded-lg text-theme-muted hover:text-error hover:bg-error-soft flex items-center justify-center transition-colors flex-shrink-0"
                    title="Remove user"
                  >
                    <UserX className="w-4 h-4" />
                  </button>
                )}
              </div>
            ))}
        </div>

        {/* Footer with user count */}
        {state === "loaded" && users.length > 0 && (
          <div className="flex items-center justify-between px-4 py-3 border-t border-theme/10 bg-theme-secondary/20">
            <span className="text-xs text-theme-muted">
              {users.length} {users.length === 1 ? "user" : "users"} with access
            </span>
            <button className="text-xs text-theme-muted hover:text-theme transition-colors flex items-center gap-1">
              <RefreshCw className="w-3 h-3" />
              Refresh
            </button>
          </div>
        )}
      </Card>
    </div>
  );
}

const meta: Meta<typeof MockDeviceUsersDialog> = {
  title: "Components/DeviceUsers",
  component: MockDeviceUsersDialog,
  tags: ["autodocs"],
  decorators: [
    (Story) => (
      <ToastProvider>
        <Story />
      </ToastProvider>
    ),
  ],
  argTypes: {
    state: {
      control: "select",
      options: ["loading", "error", "empty", "loaded"],
    },
  },
};

export default meta;
type Story = StoryObj<typeof MockDeviceUsersDialog>;

export const WithUsers: Story = {
  args: {
    deviceName: "My ECM Synchronika",
    users: mockUsers,
    state: "loaded",
  },
};

export const EmptyState: Story = {
  args: {
    deviceName: "My ECM Synchronika",
    users: [],
    state: "empty",
  },
};

export const Loading: Story = {
  args: {
    deviceName: "My ECM Synchronika",
    state: "loading",
  },
};

export const Error: Story = {
  args: {
    deviceName: "My ECM Synchronika",
    state: "error",
  },
};

export const SingleUser: Story = {
  args: {
    deviceName: "My ECM Synchronika",
    users: [mockUsers[0]],
    state: "loaded",
  },
};

export const ManyUsers: Story = {
  args: {
    deviceName: "Office Coffee Machine",
    users: [
      ...mockUsers,
      {
        userId: "user-100",
        email: "alice@example.com",
        displayName: "Alice Williams",
        avatarUrl: "https://api.dicebear.com/7.x/avataaars/svg?seed=Alice",
        claimedAt: new Date(
          Date.now() - 14 * 24 * 60 * 60 * 1000
        ).toISOString(),
      },
      {
        userId: "user-101",
        email: "charlie@example.com",
        displayName: "Charlie Brown",
        avatarUrl: null,
        claimedAt: new Date(
          Date.now() - 60 * 24 * 60 * 60 * 1000
        ).toISOString(),
      },
    ],
    state: "loaded",
  },
};

// Confirmation dialog stories
function RemoveUserConfirmationStory() {
  return (
    <ConfirmDialog
      isOpen={true}
      onClose={() => {}}
      onConfirm={() => {}}
      title="Revoke Access?"
      description="Are you sure you want to remove Jane Smith from this device? They will no longer be able to access or control it. This action cannot be undone."
      confirmText="Remove User"
      variant="danger"
    />
  );
}

export const RemoveUserConfirmation: StoryObj = {
  render: () => <RemoveUserConfirmationStory />,
};

function LeaveDeviceConfirmationStory() {
  return (
    <ConfirmDialog
      isOpen={true}
      onClose={() => {}}
      onConfirm={() => {}}
      title="Leave Device?"
      description={`Are you sure you want to leave "My ECM Synchronika"? You will no longer be able to access or control it. You can re-add it later by scanning its QR code.`}
      confirmText="Leave Device"
      variant="danger"
    />
  );
}

export const LeaveDeviceConfirmation: StoryObj = {
  render: () => <LeaveDeviceConfirmationStory />,
};

function RemoveUserConfirmationLoadingStory() {
  return (
    <ConfirmDialog
      isOpen={true}
      onClose={() => {}}
      onConfirm={() => {}}
      title="Revoke Access?"
      description="Are you sure you want to remove Jane Smith from this device? They will no longer be able to access or control it. This action cannot be undone."
      confirmText="Remove User"
      variant="danger"
      confirmLoading={true}
    />
  );
}

export const RemoveUserConfirmationLoading: StoryObj = {
  render: () => <RemoveUserConfirmationLoadingStory />,
};
