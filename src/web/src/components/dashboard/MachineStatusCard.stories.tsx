import type { Meta, StoryObj } from "@storybook/react";
import { MachineStatusCard } from "./MachineStatusCard";
import type { HeatingStrategy } from "@/lib/types";

// We need to wrap the component since it uses store
// For storybook, we'll create a mock version
const MockMachineStatusCard = (props: {
  mode: string;
  state: string;
  isDualBoiler: boolean;
  heatingStrategy?: HeatingStrategy | null;
}) => {
  return (
    <MachineStatusCard
      {...props}
      onSetMode={(mode) => console.log("Set mode:", mode)}
      onQuickOn={() => console.log("Quick on")}
      onOpenStrategyModal={() => console.log("Open strategy modal")}
    />
  );
};

const meta: Meta<typeof MockMachineStatusCard> = {
  title: "Dashboard/MachineStatusCard",
  component: MockMachineStatusCard,
  tags: ["autodocs"],
  argTypes: {
    mode: {
      control: "select",
      options: ["standby", "on", "eco"],
    },
    state: {
      control: "select",
      options: [
        "standby",
        "idle",
        "heating",
        "ready",
        "brewing",
        "steaming",
        "cooldown",
      ],
    },
    heatingStrategy: {
      control: "select",
      options: [null, 0, 1, 2, 3],
      description:
        "0: Brew Only, 1: Sequential, 2: Parallel, 3: Smart",
    },
  },
  decorators: [
    (Story) => (
      <div className="max-w-2xl">
        <Story />
      </div>
    ),
  ],
};

export default meta;
type Story = StoryObj<typeof MockMachineStatusCard>;

export const Standby: Story = {
  args: {
    mode: "standby",
    state: "standby",
    isDualBoiler: true,
    heatingStrategy: null,
  },
};

export const Heating: Story = {
  args: {
    mode: "on",
    state: "heating",
    isDualBoiler: true,
    heatingStrategy: 2,
  },
};

export const Ready: Story = {
  args: {
    mode: "on",
    state: "ready",
    isDualBoiler: true,
    heatingStrategy: 3,
  },
};

export const Brewing: Story = {
  args: {
    mode: "on",
    state: "brewing",
    isDualBoiler: true,
    heatingStrategy: 3,
  },
};

export const Steaming: Story = {
  args: {
    mode: "on",
    state: "steaming",
    isDualBoiler: true,
    heatingStrategy: 2,
  },
};

export const EcoMode: Story = {
  args: {
    mode: "eco",
    state: "ready",
    isDualBoiler: true,
    heatingStrategy: 1,
  },
};

export const SingleBoiler: Story = {
  args: {
    mode: "on",
    state: "ready",
    isDualBoiler: false,
    heatingStrategy: null,
  },
};

