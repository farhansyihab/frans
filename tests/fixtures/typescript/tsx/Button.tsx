interface Props {
    label: string;
}

export function Button(props: Props) {
    return <div>{props.label}</div>;
}
